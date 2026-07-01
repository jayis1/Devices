/**
 * MigraineSync — Aura Band Barometric (BMP390)
 * ============================================
 * License: MIT
 */

#include "baro.h"
#include "config.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <math.h>

static const struct device *i2c_dev;

#define BMP390_REG_PRESS_MSB    0x04
#define BMP390_REG_TEMP_MSB     0x07
#define BMP390_REG_PWR_CTRL     0x1B
#define BMP390_REG_OSRS         0x1F

int baro_init(void)
{
    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev))
        return -1;

    /* Enable pressure + temperature measurement */
    uint8_t cmd[2] = { BMP390_REG_PWR_CTRL, 0x03 };
    i2c_write(i2c_dev, cmd, 2, BMP390_ADDR);

    /* Set oversampling: x1 for both */
    cmd[0] = BMP390_REG_OSRS;
    cmd[1] = 0x00;
    i2c_write(i2c_dev, cmd, 2, BMP390_ADDR);

    return 0;
}

int baro_read(float *pressure_hpa, float *temp_c)
{
    if (!pressure_hpa || !temp_c)
        return -1;

    /* Read 6 bytes: pressure (3) + temperature (3) */
    uint8_t reg = BMP390_REG_PRESS_MSB;
    uint8_t data[6];
    int ret = i2c_write_read(i2c_dev, BMP390_ADDR, &reg, 1, data, 6);
    if (ret != 0)
        return -1;

    uint32_t raw_press = ((uint32_t)(data[0] & 0x0F) << 16) |
                         ((uint32_t)data[1] << 8) | data[2];
    uint32_t raw_temp  = ((uint32_t)(data[3] & 0x0F) << 16) |
                         ((uint32_t)data[4] << 8) | data[5];

    /* In production: apply BMP390 trim calibration coefficients.
     * For stub: approximate.
     */
    *pressure_hpa = (float)raw_press / 256.0f / 100.0f + 900.0f;
    *temp_c = (float)raw_temp / 256.0f / 100.0f - 20.0f;

    if (*pressure_hpa < 800 || *pressure_hpa > 1100)
        *pressure_hpa = 1013.25f;
    if (*temp_c < -20 || *temp_c > 60)
        *temp_c = 22.0f;

    return 0;
}