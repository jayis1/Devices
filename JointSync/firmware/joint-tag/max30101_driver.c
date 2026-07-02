/**
 * JointSync Joint Tag — MAX30101 PPG Driver
 *
 * I²C interface to MAX30101 pulse oximeter + heart rate sensor.
 *
 * License: MIT
 */

#include "max30101_driver.h"
#include "nrf_drv_twi.h"
#include "nrf_log.h"
#include <string.h>

#define MAX30101_TWI_INSTANCE  0
#define MAX30101_I2C_ADDR     0x57

static nrf_drv_twi_t twi = NRF_DRV_TWI_INSTANCE(MAX30101_TWI_INSTANCE);

/* MAX30101 Register Addresses */
#define MAX30101_REG_INT_STATUS_1  0x00
#define MAX30101_REG_INT_STATUS_2  0x01
#define MAX30101_REG_INT_ENABLE_1  0x02
#define MAX30101_REG_INT_ENABLE_2  0x03
#define MAX30101_REG_FIFO_WR_PTR   0x04
#define MAX30101_REG_FIFO_OV_CNTR  0x05
#define MAX30101_REG_FIFO_RD_PTR   0x06
#define MAX30101_REG_FIFO_DATA     0x07
#define MAX30101_REG_FIFO_CONFIG   0x08
#define MAX30101_REG_MODE_CONFIG   0x09
#define MAX30101_REG_SPO2_CONFIG   0x0A
#define MAX30101_REG_LED1_PA       0x0C  /* Red LED */
#define MAX30101_REG_LED2_PA       0x0D  /* IR LED */
#define MAX30101_REG_LED3_PA       0x10  /* Green LED */
#define MAX30101_REG_LED_PROX_PA   0x10
#define MAX30101_REG_PROX_INT_AMP  0x11
#define MAX30101_REG_PART_ID       0xFF

#define MAX30101_PART_ID           0x15

/* ── I²C Helpers ─────────────────────────────────────────────────── */

static nrf_err_t max30101_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = {reg, value};
    return nrf_drv_twi_tx(&twi, MAX30101_I2C_ADDR, tx, 2, false);
}

static uint8_t max30101_read_reg(uint8_t reg)
{
    uint8_t tx[1] = {reg};
    uint8_t rx[1] = {0};
    nrf_drv_twi_tx(&twi, MAX30101_I2C_ADDR, tx, 1, true);
    nrf_drv_twi_rx(&twi, MAX30101_I2C_ADDR, rx, 1);
    return rx[0];
}

static nrf_err_t max30101_read_fifo(uint8_t *buf, uint8_t len)
{
    uint8_t tx[1] = {MAX30101_REG_FIFO_DATA};
    nrf_drv_twi_tx(&twi, MAX30101_I2C_ADDR, tx, 1, true);
    return nrf_drv_twi_rx(&twi, MAX30101_I2C_ADDR, buf, len);
}

/* ── Public API ───────────────────────────────────────────────────── */

nrf_err_t max30101_init(void)
{
    /* Initialize TWI if not already done */
    nrf_drv_twi_config_t twi_config = NRF_DRV_TWI_DEFAULT_CONFIG;
    twi_config.scl = 26;  /* P0.02 */
    twi_config.sda = 27;  /* P0.03 */
    twi_config.frequency = NRF_DRV_TWI_FREQ_400K;
    nrf_drv_twi_init(&twi, &twi_config, NULL, NULL);
    nrf_drv_twi_enable(&twi);

    /* Verify part ID */
    uint8_t part_id = max30101_read_reg(MAX30101_REG_PART_ID);
    if (part_id != MAX30101_PART_ID) {
        NRF_LOG_ERROR("MAX30101 part ID mismatch: 0x%02X", part_id);
        return NRF_ERROR_INVALID_DATA;
    }

    /* Reset */
    max30101_write_reg(MAX30101_REG_MODE_CONFIG, 0x40);  /* RESET */
    nrf_delay_ms(10);

    /* Configure FIFO: 17 samples avg, rollover enabled, almost full at 17 */
    max30101_write_reg(MAX30101_REG_FIFO_CONFIG, 0x4F);

    /* Mode: SpO2 mode (Red + IR) */
    max30101_write_reg(MAX30101_REG_MODE_CONFIG, 0x03);

    /* SpO2 config: 25 Hz, 4096 ADC range, 411 µs pulse width */
    max30101_write_reg(MAX30101_REG_SPO2_CONFIG, 0x27);

    /* LED pulse amplitudes (low power) */
    max30101_write_reg(MAX30101_REG_LED1_PA, 0x24);  /* Red: 7.2 mA */
    max30101_write_reg(MAX30101_REG_LED2_PA, 0x24);  /* IR: 7.2 mA */

    /* Enable data-ready interrupt */
    max30101_write_reg(MAX30101_REG_INT_ENABLE_1, 0x80);

    NRF_LOG_INFO("MAX30101 initialized (25 Hz, SpO2 mode)");
    return NRF_SUCCESS;
}

nrf_err_t max30101_read(max30101_data_t *data)
{
    memset(data, 0, sizeof(max30101_data_t));

    /* Check for data-ready */
    uint8_t int_status = max30101_read_reg(MAX30101_REG_INT_STATUS_1);
    if (!(int_status & 0x80)) {
        return NRF_ERROR_BUSY;  /* No data ready */
    }

    /* Read FIFO: 8 samples × 6 bytes (Red 18-bit + IR 18-bit per sample) */
    uint8_t fifo_buf[48];
    nrf_err_t err = max30101_read_fifo(fifo_buf, 48);
    if (err != NRF_SUCCESS) return err;

    /* Parse 8 samples */
    for (int i = 0; i < 8; i++) {
        /* Red LED (18-bit, bytes 0-2 per sample) */
        uint8_t *p = &fifo_buf[i * 6];
        data->red_samples[i] = ((uint16_t)(p[0] & 0x03) << 14) |
                                ((uint16_t)p[1] << 6) |
                                (p[2] >> 2);

        /* IR LED (18-bit, bytes 3-5 per sample) */
        data->ir_samples[i] = ((uint16_t)(p[3] & 0x03) << 14) |
                               ((uint16_t)p[4] << 6) |
                               (p[5] >> 2);
    }

    /* Compute HR from IR signal (simple peak detection) */
    int peaks = 0;
    for (int i = 1; i < 7; i++) {
        if (data->ir_samples[i] > data->ir_samples[i-1] &&
            data->ir_samples[i] > data->ir_samples[i+1]) {
            peaks++;
        }
    }
    /* 8 samples at 25 Hz = 0.32 sec → HR = peaks / 0.32 * 60 */
    if (peaks > 0) {
        data->hr = (uint8_t)(peaks * 60 / 0.32f);
        if (data->hr > 220) data->hr = 0;  /* Invalid */
    }

    /* Compute SpO2 (simplified: ratio of ratios) */
    float red_dc = 0, ir_dc = 0;
    for (int i = 0; i < 8; i++) {
        red_dc += data->red_samples[i];
        ir_dc += data->ir_samples[i];
    }
    red_dc /= 8.0f;
    ir_dc /= 8.0f;
    if (ir_dc > 0 && red_dc > 0) {
        float ratio = (red_dc / ir_dc);
        data->spo2 = (uint8_t)(110.0f - 25.0f * ratio);
        if (data->spo2 > 100) data->spo2 = 0;
    }

    /* HRV (RMSSD) simplified — needs more samples for accuracy */
    data->hrv_ms = 0;
    data->confidence = 80;  /* Simplified confidence */

    return NRF_SUCCESS;
}

void max30101_shutdown(void)
{
    max30101_write_reg(MAX30101_REG_MODE_CONFIG, 0x80);  /* SHDN bit */
}

void max30101_wakeup(void)
{
    max30101_write_reg(MAX30101_REG_MODE_CONFIG, 0x03);  /* SpO2 mode */
}