/*
 * ErgoFlow — MMC5603 Magnetometer Driver
 * Used in wearable tag for 9-DOF sensor fusion
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "mmc5603.h"
#include "i2c_bus.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mmc5603, CONFIG_ERGO_LOG_LEVEL);

#define MMC5603_REG_XOUT0      0x00
#define MMC5603_REG_XOUT1      0x01
#define MMC5603_REG_YOUT0      0x02
#define MMC5603_REG_YOUT1      0x03
#define MMC5603_REG_ZOUT0      0x04
#define MMC5603_REG_ZOUT1      0x05
#define MMC5603_REG_STATUS      0x18
#define MMC5603_REG_CNTL0       0x1B
#define MMC5603_REG_CNTL1       0x1C
#define MMC5603_REG_CNTL2       0x1D
#define MMC5603_REG_ODR         0x1A

static uint8_t mmc5603_addr = MMC5603_I2C_ADDR;

int mmc5603_init(uint8_t i2c_addr)
{
    mmc5603_addr = i2c_addr;

    /* Verify device ID */
    uint8_t id = 0;
    int ret = i2c_bus_read_byte(mmc5603_addr, 0x39, &id);
    if (ret != 0 || id != 0x10) {
        LOG_ERR("MMC5603 not found (got 0x%02X)", id);
        return -ENODEV;
    }

    /* Set continuous mode at 25Hz ODR */
    ret = i2c_bus_write_byte(mmc5603_addr, MMC5603_REG_ODR, 0x02); /* 25Hz */
    if (ret != 0) return ret;

    /* Enable continuous measurement */
    ret = i2c_bus_write_byte(mmc5603_addr, MMC5603_REG_CNTL0, 0x80); /* CMM_EN */
    ret |= i2c_bus_write_byte(mmc5603_addr, MMC5603_REG_CNTL1, 0x00);
    ret |= i2c_bus_write_byte(mmc5603_addr, MMC5603_REG_CNTL2, 0x03); /* BW=3 */

    if (ret != 0) {
        LOG_ERR("MMC5603 config failed");
        return ret;
    }

    k_msleep(50);
    LOG_INF("MMC5603 initialized at 0x%02X", mmc5603_addr);
    return 0;
}

int mmc5603_read(mmc5603_mag_t *mag)
{
    uint8_t buf[6];
    int ret = i2c_bus_read_reg(mmc5603_addr, MMC5603_REG_XOUT0, buf, 6);
    if (ret != 0) return ret;

    /* 20-bit magnetic data */
    int32_t raw_x = ((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4);
    int32_t raw_y = ((uint32_t)buf[2] << 12) | ((uint32_t)buf[3] << 4);
    int32_t raw_z = ((uint32_t)buf[4] << 12) | ((uint32_t)buf[5] << 4);

    /* Convert to mG (sensitivity: 0.00625 mG/LSB for 20-bit) */
    mag->x = (float)raw_x * 0.00625f;
    mag->y = (float)raw_y * 0.00625f;
    mag->z = (float)raw_z * 0.00625f;

    return 0;
}