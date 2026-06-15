/*
 * ErgoFlow — LSM6DSOX 6-Axis IMU Driver
 * Used in chair pad node for tilt/motion detection
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "lsm6dsox.h"
#include "i2c_bus.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lsm6dsox, CONFIG_ERGO_LOG_LEVEL);

/* LSM6DSOX Register Map */
#define LSM6DSOX_FUNC_CFG_ACCESS   0x01
#define LSM6DSOX_FIFO_CTRL1        0x06
#define LSM6DSOX_FIFO_CTRL2        0x07
#define LSM6DSOX_FIFO_CTRL3        0x08
#define LSM6DSOX_COUNTER_BDR_REG1  0x0B
#define LSM6DSOX_INT1_CTRL          0x0D
#define LSM6DSOX_INT2_CTRL          0x0E
#define LSM6DSOX_WHO_AM_I           0x0F
#define LSM6DSOX_CTRL1_XL           0x10
#define LSM6DSOX_CTRL2_G            0x11
#define LSM6DSOX_CTRL3_C            0x12
#define LSM6DSOX_CTRL4_C            0x13
#define LSM6DSOX_CTRL5_C            0x14
#define LSM6DSOX_CTRL6_C            0x15
#define LSM6DSOX_CTRL7_G            0x16
#define LSM6DSOX_CTRL8_XL           0x17
#define LSM6DSOX_CTRL9_XL           0x18
#define LSM6DSOX_CTRL10_C           0x19
#define LSM6DSOX_STATUS_REG         0x1E
#define LSM6DSOX_OUT_TEMP_L         0x20
#define LSM6DSOX_OUTX_L_G           0x22
#define LSM6DSOX_OUTX_L_XL          0x28
#define LSM6DSOX_EMB_FUNC_EN_A      0x04
#define LSM6DSOX_PAGE_ADDRESS        0x08
#define LSM6DSOX_MD1_CFG             0x5E
#define LSM6DSOX_MD2_CFG             0x5F

/* WHO_AM_I value */
#define LSM6DSOX_WHO_AM_I_VAL  0x6C

static uint8_t lsm6dsox_addr = LSM6DSOX_I2C_ADDR;

int lsm6dsox_init(uint8_t i2c_addr)
{
    lsm6dsox_addr = i2c_addr;

    /* Verify device ID */
    uint8_t who_am_i = 0;
    int ret = i2c_bus_read_byte(lsm6dsox_addr, LSM6DSOX_WHO_AM_I, &who_am_i);
    if (ret != 0 || who_am_i != LSM6DSOX_WHO_AM_I_VAL) {
        LOG_ERR("LSM6DSOX not found (got 0x%02X, expected 0x%02X)", who_am_i, LSM6DSOX_WHO_AM_I_VAL);
        return -ENODEV;
    }

    /* Software reset */
    ret = i2c_bus_write_byte(lsm6dsox_addr, LSM6DSOX_CTRL3_C, 0x01);
    if (ret != 0) return ret;
    k_msleep(10);

    /* Wait for reset to complete */
    uint8_t ctrl3;
    do {
        ret = i2c_bus_read_byte(lsm6dsox_addr, LSM6DSOX_CTRL3_C, &ctrl3);
        if (ret != 0) return ret;
    } while (ctrl3 & 0x01);

    /* Configure accelerometer: 104Hz, ±4g */
    ret = i2c_bus_write_byte(lsm6dsox_addr, LSM6DSOX_CTRL1_XL, 0x48);
    if (ret != 0) return ret;

    /* Configure gyroscope: 104Hz, ±500dps */
    ret = i2c_bus_write_byte(lsm6dsox_addr, LSM6DSOX_CTRL2_G, 0x44);
    if (ret != 0) return ret;

    /* Enable BDU (block data update), auto-increment */
    ret = i2c_bus_write_byte(lsm6dsox_addr, LSM6DSOX_CTRL3_C, 0x44);
    if (ret != 0) return ret;

    /* Enable activity/inactivity detection on INT1 */
    ret = i2c_bus_write_byte(lsm6dsox_addr, LSM6DSOX_INT1_CTRL, 0x40); /* INT1_DRDY_XL */
    if (ret != 0) return ret;

    LOG_INF("LSM6DSOX initialized at 0x%02X", lsm6dsox_addr);
    return 0;
}

int lsm6dsox_read_accel(lsm6dsox_accel_t *accel)
{
    uint8_t buf[6];
    int ret = i2c_bus_read_reg(lsm6dsox_addr, LSM6DSOX_OUTX_L_XL, buf, 6);
    if (ret != 0) return ret;

    int16_t raw_x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t raw_y = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]);

    /* Convert to g (±4g range, sensitivity = 0.122 mg/LSB) */
    accel->x = raw_x * 0.122e-3f;
    accel->y = raw_y * 0.122e-3f;
    accel->z = raw_z * 0.122e-3f;

    return 0;
}

int lsm6dsox_read_gyro(lsm6dsox_gyro_t *gyro)
{
    uint8_t buf[6];
    int ret = i2c_bus_read_reg(lsm6dsox_addr, LSM6DSOX_OUTX_L_G, buf, 6);
    if (ret != 0) return ret;

    int16_t raw_x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t raw_y = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]);

    /* Convert to dps (±500dps, sensitivity = 17.5 mdps/LSB) */
    gyro->x = raw_x * 17.5e-3f;
    gyro->y = raw_y * 17.5e-3f;
    gyro->z = raw_z * 17.5e-3f;

    return 0;
}

int lsm6dsox_read_temperature(float *temp_c)
{
    uint8_t buf[2];
    int ret = i2c_bus_read_reg(lsm6dsox_addr, LSM6DSOX_OUT_TEMP_L, buf, 2);
    if (ret != 0) return ret;

    int16_t raw = (int16_t)((buf[1] << 8) | buf[0]);
    *temp_c = (float)raw / 256.0f + 25.0f;
    return 0;
}

int lsm6dsox_get_status(uint8_t *status)
{
    return i2c_bus_read_byte(lsm6dsox_addr, LSM6DSOX_STATUS_REG, status);
}

bool lsm6dsox_accel_data_ready(void)
{
    uint8_t status;
    if (lsm6dsox_get_status(&status) != 0) return false;
    return (status & 0x01) != 0;  /* XLDA bit */
}

bool lsm6dsox_gyro_data_ready(void)
{
    uint8_t status;
    if (lsm6dsox_get_status(&status) != 0) return false;
    return (status & 0x02) != 0;  /* GDA bit */
}

int lsm6dsox_set_accel_odr(lsm6dsox_odr_t odr)
{
    uint8_t ctrl1;
    int ret = i2c_bus_read_byte(lsm6dsox_addr, LSM6DSOX_CTRL1_XL, &ctrl1);
    if (ret != 0) return ret;
    ctrl1 = (ctrl1 & 0x0F) | ((uint8_t)odr << 4);
    return i2c_bus_write_byte(lsm6dsox_addr, LSM6DSOX_CTRL1_XL, ctrl1);
}

int lsm6dsox_set_gyro_odr(lsm6dsox_odr_t odr)
{
    uint8_t ctrl2;
    int ret = i2c_bus_read_byte(lsm6dsox_addr, LSM6DSOX_CTRL2_G, &ctrl2);
    if (ret != 0) return ret;
    ctrl2 = (ctrl2 & 0x0F) | ((uint8_t)odr << 4);
    return i2c_bus_write_byte(lsm6dsox_addr, LSM6DSOX_CTRL2_G, ctrl2);
}