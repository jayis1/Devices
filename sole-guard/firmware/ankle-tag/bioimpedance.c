/*
 * bioimpedance.c — SoleGuard ankle tag AD5940 bioimpedance driver
 *
 * AD5940 BIA (Bioelectrical Impedance Analysis) sweep at 1 kHz - 100 kHz,
 * 4-electrode configuration around the ankle. Computes a normalized
 * edema index (0-1000) from the 5 kHz impedance magnitude vs the
 * patient's personal baseline.
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include "sole_protocol.h"

LOG_MODULE_REGISTER(bioz, LOG_LEVEL_INF);

/* AD5940 register map (abbreviated) */
#define AD5940_REG_AFECON      0x2000
#define AD5940_REG_ADCDAT      0x2004
#define AD5940_REG_SWCON       0x2100
#define AD5940_REG_SIGCON      0x2104
#define AD5940_REG_PMBUF       0x2200

/* Personal baseline (centi-ohm) — set by calibrate_insole.py / clinician */
static uint32_t baseline_impedance_ohm = 85000; /* typical ankle ~85 ohm * 1000 */

void bioz_set_baseline(uint32_t baseline)
{
    baseline_impedance_ohm = baseline;
}

static int ad5940_write(const struct device *spi, uint16_t reg, uint32_t val)
{
    uint8_t tx[5] = {
        (uint8_t)(reg & 0xFF),
        (uint8_t)((reg >> 8) & 0xFF),
        (uint8_t)(val & 0xFF),
        (uint8_t)((val >> 8) & 0xFF),
        (uint8_t)((val >> 16) & 0xFF),
    };
    struct spi_buf buf = { .buf = tx, .len = sizeof(tx) };
    struct spi_buf_set set = { .buffers = &buf, .count = 1 };
    return spi_write(spi, &set, NULL);
}

static int ad5940_read(const struct device *spi, uint16_t reg, uint32_t *val)
{
    uint8_t tx[2] = { (uint8_t)(reg & 0xFF), (uint8_t)((reg >> 8) & 0xFF) };
    uint8_t rx[3] = {0};
    struct spi_buf tx_buf = { .buf = tx, .len = sizeof(tx) };
    struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    struct spi_buf rx_buf = { .buf = rx, .len = sizeof(rx) };
    struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };
    int rc = spi_write(spi, &set, NULL);
    if (rc) return rc;
    spi_read(spi, &tx_set, &rx_set);
    *val = (uint32_t)rx[0] | ((uint32_t)rx[1] << 8) | ((uint32_t)rx[2] << 16);
    return 0;
}

int bioz_init(const struct device *spi)
{
    if (!device_is_ready(spi)) return -1;
    /* Configure AD5940 for 4-electrode BIA, 5 kHz excitation, PGA gain 1.5 */
    ad5940_write(spi, AD5940_REG_SWCON,   0x0003A0); /* CE0/RE0 swap for 4-wire */
    ad5940_write(spi, AD5940_REG_SIGCON,  0x001388); /* 5 kHz sine, 200mVpp */
    ad5940_write(spi, AD5940_REG_AFECON,  0x000010); /* BIA mode */
    LOG_INF("AD5940 BIA initialized");
    return 0;
}

int bioz_measure(uint16_t *impedance_ohm, uint16_t *edema_index)
{
    const struct device *spi = DEVICE_DT_GET(DT_NODELABEL(spi1));
    uint32_t raw;
    if (ad5940_read(spi, AD5940_REG_ADCDAT, &raw)) return -1;

    /* Convert ADC code to impedance magnitude (ohm).
       AD5940 BIA: Z = (raw / 2^15) * RTIA * gain. RTIA = 200k, gain = 1.5 */
    float z = ((float)(int32_t)raw / 32768.0f) * 200000.0f * 1.5f;
    if (z < 0) z = -z;
    uint16_t z_ohm = (uint16_t)MIN(z, 65535.0f);
    *impedance_ohm = z_ohm;

    /* Edema index: lower impedance = more fluid = higher edema.
       Normalize against personal baseline: idx = clamp(1000 * (1 - z/baseline), 0, 1000) */
    float ratio = (float)z_ohm / (float)baseline_impedance_ohm;
    float idx_f = 1000.0f * (1.0f - ratio);
    if (idx_f < 0) idx_f = 0;
    if (idx_f > 1000) idx_f = 1000;
    *edema_index = (uint16_t)idx_f;
    return 0;
}

void bioz_sleep(void)
{
    const struct device *spi = DEVICE_DT_GET(DT_NODELABEL(spi1));
    ad5940_write(spi, AD5940_REG_AFECON, 0x000000); /* power down AFE */
}