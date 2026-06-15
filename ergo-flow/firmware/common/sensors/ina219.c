/*
 * ErgoFlow — INA219 Current Sensor Driver
 * Used in desk controller for motor current monitoring
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "ina219.h"
#include "i2c_bus.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ina219, CONFIG_ERGO_LOG_LEVEL);

#define INA219_REG_CONFIG      0x00
#define INA219_REG_SHUNT_V    0x01
#define INA219_REG_BUS_V      0x02
#define INA219_REG_POWER      0x03
#define INA219_REG_CURRENT     0x04
#define INA219_REG_CALIB       0x05

#define INA219_CFG_RESET       0x8000
#define INA219_CFG_BUS_VOLT    0x0000  /* 16V range */
#define INA219_CFG_SHUNT_320MV 0x1800 /* ±320mV shunt */
#define INA219_CFG_SHUNT_160MV 0x1000 /* ±160mV shunt */
#define INA219_CFG_12BIT       0x0040  /* 12-bit, 532μs */
#define INA219_CFG_CONT        0x0007  /* Continuous shunt+bus */

/* Calibration for 0.1 ohm shunt, ±320mV, 3.2A max */
#define INA219_CALIB_VALUE     8192

static uint8_t ina219_addr = INA219_ADDR_GND;
static float current_lsb = 0.0001f;  /* 100μA LSB */
static float power_lsb = 0.002f;      /* 2mW LSB */

int ina219_init(uint8_t i2c_addr)
{
    ina219_addr = i2c_addr;

    /* Configure: 16V bus, ±320mV shunt, 12-bit, continuous */
    uint8_t config_buf[2];
    uint16_t config = INA219_CFG_BUS_VOLT | INA219_CFG_SHUNT_320MV |
                      INA219_CFG_12BIT | INA219_CFG_CONT;
    config_buf[0] = (config >> 8) & 0xFF;
    config_buf[1] = config & 0xFF;
    int ret = i2c_bus_write_reg(ina219_addr, INA219_REG_CONFIG, config_buf, 2);
    if (ret != 0) {
        LOG_ERR("INA219 config failed");
        return ret;
    }

    /* Set calibration register */
    uint8_t calib_buf[2];
    calib_buf[0] = (INA219_CALIB_VALUE >> 8) & 0xFF;
    calib_buf[1] = INA219_CALIB_VALUE & 0xFF;
    ret = i2c_bus_write_reg(ina219_addr, INA219_REG_CALIB, calib_buf, 2);
    if (ret != 0) {
        LOG_ERR("INA219 calibration failed");
        return ret;
    }

    k_msleep(1);
    LOG_INF("INA219 initialized at 0x%02X", ina219_addr);
    return 0;
}

int ina219_read_current(float *current_ma)
{
    uint8_t buf[2];
    int ret = i2c_bus_read_reg(ina219_addr, INA219_REG_CURRENT, buf, 2);
    if (ret != 0) return ret;

    int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
    *current_ma = (float)raw * current_lsb * 1000.0f;
    return 0;
}

int ina219_read_bus_voltage(float *voltage_v)
{
    uint8_t buf[2];
    int ret = i2c_bus_read_reg(ina219_addr, INA219_REG_BUS_V, buf, 2);
    if (ret != 0) return ret;

    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    raw &= 0x7FFF;  /* Mask CNVR bit */
    *voltage_v = (float)raw * 0.004f;  /* 4mV LSB */
    return 0;
}

int ina219_read_power(float *power_mw)
{
    uint8_t buf[2];
    int ret = i2c_bus_read_reg(ina219_addr, INA219_REG_POWER, buf, 2);
    if (ret != 0) return ret;

    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    *power_mw = (float)raw * power_lsb * 1000.0f;
    return 0;
}