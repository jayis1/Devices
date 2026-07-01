/**
 * MigraineSync — Aura Band Light Sensor (VEML7700)
 * ================================================
 * License: MIT
 */

#include "light.h"
#include "config.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

static const struct device *i2c_dev;

#define VEML7700_REG_CONFIG  0x00
#define VEML7700_REG_ALS     0x04

int light_init(void)
{
    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev))
        return -1;

    /* Config: gain=1x, IT=25ms, no interrupt */
    uint8_t cmd[3] = { VEML7700_REG_CONFIG, 0x00, 0x00 };
    i2c_write(i2c_dev, cmd, 3, VEML7700_ADDR);

    return 0;
}

int light_read(float *lux)
{
    if (!lux)
        return -1;

    uint8_t reg = VEML7700_REG_ALS;
    uint8_t data[2];
    int ret = i2c_write_read(i2c_dev, VEML7700_ADDR, &reg, 1, data, 2);
    if (ret != 0)
        return -1;

    uint16_t raw = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    /* With gain=1x, IT=25ms: lux = raw * 0.0036 */
    *lux = raw * 0.0036f;
    return 0;
}