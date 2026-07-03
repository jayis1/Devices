/**
 * DriveSync PPG Driver — MAX30101
 *
 * Maxim Integrated MAX30101: reflective PPG sensor.
 * I2C address: 0x57 (ADDR pin tied low).
 * Green + IR LEDs for heart rate and SpO2 measurement.
 *
 * License: MIT
 */

#include "ppg_driver.h"
#include "nrf_drv_twi.h"
#include "nrf_log.h"
#include <string.h>

#define MAX30101_ADDR      0x57
#define REG_INTR_STATUS_1  0x00
#define REG_INTR_STATUS_2  0x01
#define REG_INTR_ENABLE_1  0x02
#define REG_INTR_ENABLE_2  0x03
#define REG_FIFO_WR_PTR    0x04
#define REG_OVF_COUNTER    0x05
#define REG_FIFO_RD_PTR    0x06
#define REG_FIFO_DATA      0x07
#define REG_MODE_CONFIG    0x09
#define REG_SPO2_CONFIG    0x0A
#define REG_LED1_PA        0x0C  /* Red LED */
#define REG_LED2_PA        0x0D  /* IR LED */
#define REG_LED3_PA        0x0E  /* Green LED */
#define REG_PART_ID        0xFF

static nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(1);

static void twi_init(void)
{
    nrf_drv_twi_config_t twi_config = {
        .scl = 25,
        .sda = 24,
        .frequency = NRF_TWI_FREQ_400K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
        .clear_bus_init = false,
    };
    nrf_drv_twi_init(&m_twi, &twi_config, NULL, NULL);
    nrf_drv_twi_enable(&m_twi);
}

static void max_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    nrf_drv_twi_tx(&m_twi, MAX30101_ADDR, buf, 2, false);
}

static uint8_t max_read_reg(uint8_t reg)
{
    uint8_t val = 0;
    nrf_drv_twi_tx(&m_twi, MAX30101_ADDR, &reg, 1, true);
    nrf_drv_twi_rx(&m_twi, MAX30101_ADDR, &val, 1);
    return val;
}

void ppg_init(void)
{
    twi_init();

    /* Reset */
    max_write_reg(REG_MODE_CONFIG, 0x40);
    nrf_delay_ms(10);

    /* Verify part ID */
    uint8_t part_id = max_read_reg(REG_PART_ID);
    if (part_id != 0x15) {
        NRF_LOG_ERROR("MAX30101 not found (part_id=0x%02X)", part_id);
        return;
    }

    /* Clear interrupts */
    max_read_reg(REG_INTR_STATUS_1);
    max_read_reg(REG_INTR_STATUS_2);

    /* Enable FIFO A-full interrupt */
    max_write_reg(REG_INTR_ENABLE_1, 0x80);
    max_write_reg(REG_INTR_ENABLE_2, 0x00);

    /* Configure FIFO: avg 8 samples, rollover on, almost full at 17 */
    max_write_reg(0x08, 0x4F);  /* FIFO_CONFIG */

    /* Mode: SpO2 mode (red + IR) */
    max_write_reg(REG_MODE_CONFIG, 0x03);

    /* SpO2 config: 1600 Hz, 4096 sample width, 411 us LED pulse */
    max_write_reg(REG_SPO2_CONFIG, 0x27);

    /* LED pulse amplitudes: 7.0 mA (0x40 = 12.8 mA) */
    max_write_reg(REG_LED1_PA, 0x40);  /* Red */
    max_write_reg(REG_LED2_PA, 0x40);  /* IR */
    max_write_reg(REG_LED3_PA, 0x40);  /* Green */

    NRF_LOG_INFO("MAX30101 PPG initialized (25 Hz, SpO2 mode)");
}

int ppg_read(ppg_data_t *data)
{
    if (data == NULL) return -1;

    memset(data, 0, sizeof(ppg_data_t));

    /* Read 8 samples from FIFO (each sample = 6 bytes: red[3] + IR[3]) */
    uint8_t raw[48];
    uint8_t reg = REG_FIFO_DATA;
    nrf_drv_twi_tx(&m_twi, MAX30101_ADDR, &reg, 1, true);
    nrf_drv_twi_rx(&m_twi, MAX30101_ADDR, raw, 48);

    for (uint8_t i = 0; i < 8; i++) {
        /* Red LED (18-bit) */
        data->red_samples[i] = ((uint16_t)raw[i*6] << 8) | raw[i*6+1];
        /* IR LED (18-bit) */
        data->ir_samples[i] = ((uint16_t)raw[i*6+3] << 8) | raw[i*6+4];
    }

    data->hr = 0;
    data->spo2 = 0;
    data->confidence = 75;  /* Stub */

    return 0;
}

uint8_t ppg_compute_spo2(const uint16_t *ir, const uint16_t *red, uint8_t count)
{
    if (count == 0) return 0;

    /* Simplified SpO2: ratio of ratios (R) */
    /* R = (AC_red / DC_red) / (AC_ir / DC_ir) */
    /* SpO2 = 110 - 25 * R (empirical formula) */

    uint32_t ir_max = 0, ir_min = 0xFFFF;
    uint32_t red_max = 0, red_min = 0xFFFF;
    uint32_t ir_sum = 0, red_sum = 0;

    for (uint8_t i = 0; i < count; i++) {
        if (ir[i] > ir_max) ir_max = ir[i];
        if (ir[i] < ir_min) ir_min = ir[i];
        if (red[i] > red_max) red_max = red[i];
        if (red[i] < red_min) red_min = red[i];
        ir_sum += ir[i];
        red_sum += red[i];
    }

    uint32_t ir_dc = ir_sum / count;
    uint32_t red_dc = red_sum / count;

    if (ir_dc == 0 || red_dc == 0) return 0;

    uint32_t ir_ac = ir_max - ir_min;
    uint32_t red_ac = red_max - red_min;

    float R = ((float)red_ac / red_dc) / ((float)ir_ac / ir_dc);
    float spo2 = 110.0f - 25.0f * R;

    if (spo2 > 100.0f) spo2 = 100.0f;
    if (spo2 < 70.0f) spo2 = 70.0f;

    return (uint8_t)spo2;
}