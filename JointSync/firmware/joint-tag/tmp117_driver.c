/**
 * JointSync Joint Tag — TMP117 Temperature Driver
 *
 * I²C interface to TMP117 (±0.1°C medical-grade digital temp sensor).
 *
 * License: MIT
 */

#include "tmp117_driver.h"
#include "nrf_drv_twi.h"
#include "nrf_log.h"

#define TMP117_TWI_INSTANCE  0
#define TMP117_I2C_ADDR     0x48  /* ADDR pin tied to GND */

static nrf_drv_twi_t twi = NRF_DRV_TWI_INSTANCE(TMP117_TWI_INSTANCE);

/* TMP117 Register Addresses */
#define TMP117_REG_TEMP      0x00
#define TMP117_REG_CONFIG    0x01
#define TMP117_REG_THIGH     0x02
#define TMP117_REG_TLOW      0x03

/* ── I²C Helpers ─────────────────────────────────────────────────── */

static nrf_err_t tmp117_write_reg(uint8_t reg, uint16_t value)
{
    uint8_t tx[3] = {reg, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
    return nrf_drv_twi_tx(&twi, TMP117_I2C_ADDR, tx, 3, false);
}

static uint16_t tmp117_read_reg(uint8_t reg)
{
    uint8_t tx[1] = {reg};
    uint8_t rx[2] = {0};
    nrf_drv_twi_tx(&twi, TMP117_I2C_ADDR, tx, 1, true);
    nrf_drv_twi_rx(&twi, TMP117_I2C_ADDR, rx, 2);
    return ((uint16_t)rx[0] << 8) | rx[1];
}

/* ── Public API ───────────────────────────────────────────────────── */

nrf_err_t tmp117_init(void)
{
    /* TWI already initialized by MAX30101 or separately */
    /* Configure TMP117: continuous conversion, 1 Hz, 10 averaging */
    uint16_t config = 0x0C20;  /* Continuous, 10 avg, 1 sec conversion */
    return tmp117_write_reg(TMP117_REG_CONFIG, config);
}

int16_t tmp117_read_temp(void)
{
    uint16_t raw = tmp117_read_reg(TMP117_REG_TEMP);
    /* TMP117: 0.0078125 °C per LSB (7.8125 m°C) */
    /* Convert to centi-degrees: raw * 0.78125 */
    int32_t centi = ((int16_t)raw * 78125) / 100000;
    if (centi > 32767) centi = 32767;
    if (centi < -32768) centi = -32768;
    return (int16_t)centi;
}

int16_t tmp117_read_ambient(void)
{
    /* Read from secondary TMP117 at address 0x49 if present */
    uint8_t tx[1] = {TMP117_REG_TEMP};
    uint8_t rx[2] = {0};
    nrf_drv_twi_tx(&twi, 0x49, tx, 1, true);
    nrf_err_t err = nrf_drv_twi_rx(&twi, 0x49, rx, 2);
    if (err != NRF_SUCCESS) return 0;

    uint16_t raw = ((uint16_t)rx[0] << 8) | rx[1];
    int32_t centi = ((int16_t)raw * 78125) / 100000;
    return (int16_t)centi;
}

void tmp117_set_alert(int16_t high_centi, int16_t low_centi)
{
    /* Convert centi-degrees to TMP117 raw (1 LSB = 0.0078125°C) */
    uint16_t high_raw = (uint16_t)(high_centi / 0.78125);
    uint16_t low_raw = (uint16_t)(low_centi / 0.78125);
    tmp117_write_reg(TMP117_REG_THIGH, high_raw);
    tmp117_write_reg(TMP117_REG_TLOW, low_raw);
}