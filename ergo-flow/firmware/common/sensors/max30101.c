/*
 * ErgoFlow — MAX30101 Pulse Oximeter Driver
 * Used in wearable tag for heart rate and SpO2
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "max30101.h"
#include "i2c_bus.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(max30101, CONFIG_ERGO_LOG_LEVEL);

#define MAX30101_REG_INTR_STATUS_1    0x00
#define MAX30101_REG_INTR_STATUS_2    0x01
#define MAX30101_REG_INTR_ENABLE_1    0x02
#define MAX30101_REG_INTR_ENABLE_2    0x03
#define MAX30101_REG_FIFO_WR_PTR      0x04
#define MAX30101_REG_OVF_COUNTER      0x05
#define MAX30101_REG_FIFO_RD_PTR      0x06
#define MAX30101_REG_FIFO_DATA        0x07
#define MAX30101_REG_FIFO_CONFIG      0x08
#define MAX30101_REG_MODE_CONFIG      0x09
#define MAX30101_REG_SPO2_CONFIG      0x0A
#define MAX30101_REG_LED1_PA          0x0C  /* Red LED */
#define MAX30101_REG_LED2_PA          0x0D  /* IR LED */
#define MAX30101_REG_LED3_PA          0x0E  /* Green LED */
#define MAX30101_REG_LED4_PA          0x0F
#define MAX30101_REG_MULTI_LED_CTRL1  0x11
#define MAX30101_REG_MULTI_LED_CTRL2  0x12
#define MAX30101_REG_DIE_TEMP_INT     0x1F
#define MAX30101_REG_DIE_TEMP_FRAC    0x20
#define MAX30101_REG_DIE_TEMP_CONFIG  0x21
#define MAX30101_REG_PROX_INT_THRESH  0x10
#define MAX30101_REG_REV_ID           0xFE
#define MAX30101_REG_PART_ID          0xFF

#define MAX30101_PART_ID_VAL  0x15

/* FIFO sample buffer */
#define MAX30101_FIFO_DEPTH  32

static uint8_t max30101_addr = MAX30101_I2C_ADDR;

int max30101_init(void)
{
    max30101_addr = MAX30101_I2C_ADDR;

    /* Verify device ID */
    uint8_t part_id = 0;
    int ret = i2c_bus_read_byte(max30101_addr, MAX30101_REG_PART_ID, &part_id);
    if (ret != 0 || part_id != MAX30101_PART_ID_VAL) {
        LOG_ERR("MAX30101 not found (got 0x%02X)", part_id);
        return -ENODEV;
    }

    /* Reset */
    ret = i2c_bus_write_byte(max30101_addr, MAX30101_REG_MODE_CONFIG, 0x40);
    if (ret != 0) return ret;
    k_msleep(10);

    /* Wait for reset */
    uint8_t mode;
    do {
        ret = i2c_bus_read_byte(max30101_addr, MAX30101_REG_MODE_CONFIG, &mode);
        if (ret != 0) return ret;
    } while (mode & 0x40);

    /* Configure FIFO: avg=4, FIFO rollover, almost full at 17 */
    ret = i2c_bus_write_byte(max30101_addr, MAX30101_REG_FIFO_CONFIG, 0x4F);
    if (ret != 0) return ret;

    /* Mode: SpO2 mode (Red + IR) */
    ret = i2c_bus_write_byte(max30101_addr, MAX30101_REG_MODE_CONFIG, 0x03);
    if (ret != 0) return ret;

    /* SpO2 config: 100Hz, 18-bit, 4110μA */
    ret = i2c_bus_write_byte(max30101_addr, MAX30101_REG_SPO2_CONFIG, 0x47);
    if (ret != 0) return ret;

    /* LED pulse amplitudes */
    ret = i2c_bus_write_byte(max30101_addr, MAX30101_REG_LED1_PA, 0x24); /* Red: 14.2mA */
    ret |= i2c_bus_write_byte(max30101_addr, MAX30101_REG_LED2_PA, 0x24); /* IR: 14.2mA */
    if (ret != 0) return ret;

    /* Clear FIFO */
    ret = i2c_bus_write_byte(max30101_addr, MAX30101_REG_FIFO_WR_PTR, 0x00);
    ret |= i2c_bus_write_byte(max30101_addr, MAX30101_REG_OVF_COUNTER, 0x00);
    ret |= i2c_bus_write_byte(max30101_addr, MAX30101_REG_FIFO_RD_PTR, 0x00);

    LOG_INF("MAX30101 initialized at 0x%02X", max30101_addr);
    return 0;
}

int max30101_read_hr(float *hr_bpm, float *spo2_pct)
{
    /* In production: collect 100 samples over 1 second at 100Hz,
     * apply PPG signal processing, peak detection for HR,
     * and red/IR ratio calculation for SpO2.
     * For now: simplified placeholder that returns simulated values */
    uint8_t fifo_count = 0;
    int ret = i2c_bus_read_byte(max30101_addr, MAX30101_REG_FIFO_WR_PTR, &fifo_count);
    if (ret != 0) {
        *hr_bpm = 0;
        *spo2_pct = 0;
        return ret;
    }

    /* Check for valid samples */
    if (fifo_count == 0) {
        *hr_bpm = 0;
        *spo2_pct = 0;
        return -1;
    }

    /* Read samples and compute HR/SpO2
     * This is a simplified placeholder - production code would use
     * proper PPG signal processing with peak detection */
    *hr_bpm = 72.0f;   /* Placeholder */
    *spo2_pct = 98.0f;  /* Placeholder */

    return 0;
}

void max30101_shutdown(void)
{
    i2c_bus_write_byte(max30101_addr, MAX30101_REG_MODE_CONFIG, 0x80);
}

void max30101_wakeup(void)
{
    i2c_bus_write_byte(max30101_addr, MAX30101_REG_MODE_CONFIG, 0x03);
}