/*
 * ErgoFlow — I2C Bus Management
 * Shared I2C initialization and device scanning
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "i2c_bus.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(i2c_bus, CONFIG_ERGO_LOG_LEVEL);

#define I2C_NODE DT_ALIAS(i2c0)

static const struct device *i2c_dev;
static bool initialized = false;

int i2c_bus_init(void)
{
    i2c_dev = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return -ENODEV;
    }

    /* Configure I2C bus speed */
    int err = i2c_configure(i2c_dev, I2C_SPEED_FAST);
    if (err) {
        LOG_ERR("I2C configure failed: %d", err);
        return err;
    }

    initialized = true;
    LOG_INF("I2C bus initialized at 400kHz");
    return 0;
}

int i2c_bus_scan(void)
{
    if (!initialized) return -ENODEV;

    LOG_INF("Scanning I2C bus...");
    int found = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        int ret = i2c_probe(i2c_dev, addr);
        if (ret == 0) {
            LOG_INF("  Found device at 0x%02X", addr);
            found++;
        }
    }

    LOG_INF("I2C scan complete: %d device(s) found", found);
    return found;
}

int i2c_bus_write_reg(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
    if (!initialized) return -ENODEV;
    uint8_t buf[len + 1];
    buf[0] = reg;
    memcpy(&buf[1], data, len);
    return i2c_write(i2c_dev, buf, len + 1, dev_addr);
}

int i2c_bus_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!initialized) return -ENODEV;
    return i2c_write_read(i2c_dev, dev_addr, &reg, 1, data, len);
}

int i2c_bus_write_byte(uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    return i2c_bus_write_reg(dev_addr, reg, &val, 1);
}

int i2c_bus_read_byte(uint8_t dev_addr, uint8_t reg, uint8_t *val)
{
    return i2c_bus_read_reg(dev_addr, reg, val, 1);
}