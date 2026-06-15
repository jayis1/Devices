/*
 * ErgoFlow — SHT40 Temperature/Humidity Sensor Driver
 * Used in hub node for environmental monitoring
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "sht40.h"
#include "i2c_bus.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sht40, CONFIG_ERGO_LOG_LEVEL);

#define SHT40_CMD_MEASURE_HPM   0xFD   /* High precision measurement */
#define SHT40_CMD_MEASURE_MPM   0xF5   /* Medium precision measurement */
#define SHT40_CMD_MEASURE_LPM   0xE0   /* Low precision measurement */
#define SHT40_CMD_HEATER_200MW  0x39   /* 200mW heater for 1s */
#define SHT40_CMD_RESET         0x94   /* Soft reset */

static uint8_t sht40_addr = SHT40_ADDR;

/* CRC-8 calculation (polynomial 0x31, init 0xFF) */
static uint8_t sht40_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0xFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}

int sht40_init(uint8_t i2c_addr)
{
    sht40_addr = i2c_addr;

    /* Soft reset */
    uint8_t cmd = SHT40_CMD_RESET;
    int ret = i2c_bus_write_reg(sht40_addr, cmd, NULL, 0);
    if (ret != 0) {
        LOG_ERR("SHT40 reset failed");
        return ret;
    }
    k_msleep(5);

    LOG_INF("SHT40 initialized at 0x%02X", sht40_addr);
    return 0;
}

int sht40_read(float *temp_c, float *humidity_pct)
{
    /* Send measurement command */
    uint8_t cmd = SHT40_CMD_MEASURE_HPM;
    int ret = i2c_bus_write_reg(sht40_addr, cmd, NULL, 0);
    if (ret != 0) return ret;

    /* Wait for measurement (typical 7ms for high precision) */
    k_msleep(10);

    /* Read 6 bytes: temp_msb, temp_lsb, temp_crc, hum_msb, hum_lsb, hum_crc */
    uint8_t buf[6];
    ret = i2c_bus_read_reg(sht40_addr, 0x00, buf, 6);
    /* SHT40 uses repeated start, so we read directly */
    uint8_t dummy = 0;
    ret = i2c_bus_write_reg(sht40_addr, cmd, NULL, 0);
    if (ret != 0) return ret;
    k_msleep(10);

    /* Use I2C read after command */
    const struct device *i2c_dev = DEVICE_DT_GET(DT_ALIAS(i2c0));
    ret = i2c_read(i2c_dev, buf, 6, sht40_addr);
    if (ret != 0) return ret;

    /* Verify CRC */
    if (sht40_crc8(buf, 2) != buf[2]) {
        LOG_ERR("SHT40 temperature CRC mismatch");
        return -EIO;
    }
    if (sht40_crc8(buf + 3, 2) != buf[5]) {
        LOG_ERR("SHT40 humidity CRC mismatch");
        return -EIO;
    }

    /* Convert to physical values */
    uint16_t raw_temp = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t raw_hum = ((uint16_t)buf[3] << 8) | buf[4];

    *temp_c = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    *humidity_pct = -6.0f + 125.0f * ((float)raw_hum / 65535.0f);

    /* Clamp humidity to valid range */
    if (*humidity_pct < 0.0f) *humidity_pct = 0.0f;
    if (*humidity_pct > 100.0f) *humidity_pct = 100.0f;

    return 0;
}