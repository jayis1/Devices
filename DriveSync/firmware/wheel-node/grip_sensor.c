/**
 * DriveSync Grip Sensor Driver — FDC2214
 *
 * Texas Instruments FDC2214: 4-channel capacitance-to-digital converter.
 * I2C address: 0x2A (ADDR pin low).
 * Resolution: 28-bit, fCLK up to 35 MHz.
 *
 * License: MIT
 */

#include "grip_sensor.h"
#include "nrf_drv_twi.h"
#include "nrf_log.h"

#define FDC2214_ADDR       0x2A
#define FDC2214_RCOUNT0    0x08
#define FDC2214_RCOUNT1    0x09
#define FDC2214_RCOUNT2    0x0A
#define FDC2214_RCOUNT3    0x0B
#define FDC2214_OFFSET0    0x0C
#define FDC2214_SETTLE0    0x10
#define FDC2214_CLOCK_DIV  0x14
#define FDC2214_STATUS     0x18
#define FDC2214_RCOUNT_CH(ch) (0x08 + 2*ch)
#define FDC2214_DATA_MSB(ch)  (0x00 + 2*ch)
#define FDC2214_DATA_LSB(ch)  (0x01 + 2*ch)

static nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(2);
static uint32_t s_baseline = 0;

static void twi_init(void)
{
    nrf_drv_twi_config_t twi_config = {
        .scl = 28,
        .sda = 27,
        .frequency = NRF_TWI_FREQ_400K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
        .clear_bus_init = false,
    };
    nrf_drv_twi_init(&m_twi, &twi_config, NULL, NULL);
    nrf_drv_twi_enable(&m_twi);
}

static void fdc_write_reg(uint8_t reg, uint16_t val)
{
    uint8_t buf[3] = {reg, (uint8_t)(val >> 8), (uint8_t)(val & 0xFF)};
    nrf_drv_twi_tx(&m_twi, FDC2214_ADDR, buf, 3, false);
}

static uint16_t fdc_read_reg(uint8_t reg)
{
    uint8_t val[2] = {0};
    nrf_drv_twi_tx(&m_twi, FDC2214_ADDR, &reg, 1, true);
    nrf_drv_twi_rx(&m_twi, FDC2214_ADDR, val, 2);
    return (uint16_t)((val[0] << 8) | val[1]);
}

static uint32_t fdc_read_channel(uint8_t ch)
{
    uint16_t msb = fdc_read_reg(FDC2214_DATA_MSB(ch));
    uint16_t lsb = fdc_read_reg(FDC2214_DATA_LSB(ch));
    /* 28-bit value: MSB[11:0] << 16 | LSB[15:0] */
    return ((uint32_t)(msb & 0x0FFF) << 16) | lsb;
}

void grip_sensor_init(void)
{
    twi_init();

    /* Set RCOUNT for maximum resolution (0xFFFF) */
    for (uint8_t ch = 0; ch < 4; ch++) {
        fdc_write_reg(FDC2214_RCOUNT_CH(ch), 0xFFFF);
    }

    /* Set settling time */
    fdc_write_reg(FDC2214_SETTLE0, 0x0400);

    /* Set clock divider */
    fdc_write_reg(FDC2214_CLOCK_DIV, 0x1001);

    /* Enable error config */
    fdc_write_reg(FDC2214_ERROR_CONFIG, 0x0001);

    /* Set mux configuration for 4 channels */
    fdc_write_reg(FDC2214_MUX_CONFIG, 0x020F);

    /* Enable all channels */
    fdc_write_reg(FDC2214_CLOCK_DIV, 0x5001);

    /* Drive current */
    fdc_write_reg(FDC2214_DRIVE_CURRENT0, 0x9000);

    NRF_LOG_INFO("FDC2214 grip sensor initialized (4-channel)");
}

void grip_sensor_read(uint16_t values[4])
{
    for (uint8_t ch = 0; ch < 4; ch++) {
        /* Read 28-bit value, truncate to 16 bits for BLE payload */
        uint32_t raw = fdc_read_channel(ch);
        values[ch] = (uint16_t)(raw >> 12);  /* Top 16 bits */
    }
}

void grip_sensor_calibrate(void)
{
    uint16_t vals[4];
    grip_sensor_read(vals);
    s_baseline = (uint32_t)vals[0] + vals[1] + vals[2] + vals[3];
    NRF_LOG_INFO("Grip baseline: %u", s_baseline);
}

uint8_t grip_sensor_get_hands_on(void)
{
    uint16_t vals[4];
    grip_sensor_read(vals);
    uint32_t sum = (uint32_t)vals[0] + vals[1] + vals[2] + vals[3];
    return (sum > s_baseline + 100) ? 1 : 0;
}