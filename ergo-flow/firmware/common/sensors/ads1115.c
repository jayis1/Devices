/*
 * ErgoFlow — ADS1115 16-bit ADC Driver
 * Used in chair pad node for FSR pressure sensor scanning
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "ads1115.h"
#include "i2c_bus.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ads1115, CONFIG_ERGO_LOG_LEVEL);

/* ADS1115 Register Map */
#define ADS1115_REG_CONV    0x00
#define ADS1115_REG_CONFIG  0x01
#define ADS1115_REG_LO_THRESH 0x02
#define ADS1115_REG_HI_THRESH 0x03

/* Config register bits */
#define ADS1115_CFG_OS_START    (1 << 15)
#define ADS1115_CFG_MUX_AIN0_G  (0x4 << 12)  /* AIN0 vs GND */
#define ADS1115_CFG_MUX_AIN1_G  (0x5 << 12)  /* AIN1 vs GND */
#define ADS1115_CFG_MUX_AIN2_G  (0x6 << 12)  /* AIN2 vs GND */
#define ADS1115_CFG_MUX_AIN3_G  (0x7 << 12)  /* AIN3 vs GND */
#define ADS1115_CFG_PGA_4V096   (0x1 << 9)    /* ±4.096V range */
#define ADS1115_CFG_PGA_2V048   (0x2 << 9)    /* ±2.048V range */
#define ADS1115_CFG_MODE_SINGLE (0x1 << 8)    /* Single-shot */
#define ADS1115_CFG_DR_860SPS  (0x7 << 5)     /* 860 SPS */
#define ADS1115_CFG_DR_128SPS  (0x4 << 5)     /* 128 SPS */
#define ADS1115_CFG_COMP_1     (0x0 << 4)     /* Traditional comparator */
#define ADS1115_CFG_COMP_LATCH (0x0 << 2)     /* Non-latching */
#define ADS1115_CFG_COMP_QUE_1 (0x3 << 0)     /* Disable comparator */

static uint8_t ads1115_addr = ADS1115_ADDR_GND;

int ads1115_init(uint8_t i2c_addr)
{
    ads1115_addr = i2c_addr;

    /* Verify device presence */
    uint8_t config_msb = 0;
    int ret = i2c_bus_read_byte(ads1115_addr, ADS1115_REG_CONFIG, &config_msb);
    if (ret != 0) {
        LOG_ERR("ADS1115 not found at 0x%02X", ads1115_addr);
        return ret;
    }

    LOG_INF("ADS1115 initialized at 0x%02X", ads1115_addr);
    return 0;
}

int ads1115_read_channel(uint8_t channel, int16_t *result)
{
    uint16_t config;
    uint8_t buf[2];

    /* Read current config */
    int ret = i2c_bus_read_reg(ads1115_addr, ADS1115_REG_CONFIG, buf, 2);
    if (ret != 0) return ret;
    config = ((uint16_t)buf[0] << 8) | buf[1];

    /* Set MUX for single-ended channel */
    config &= ~(0x7 << 12);
    switch (channel) {
        case 0: config |= ADS1115_CFG_MUX_AIN0_G; break;
        case 1: config |= ADS1115_CFG_MUX_AIN1_G; break;
        case 2: config |= ADS1115_CFG_MUX_AIN2_G; break;
        case 3: config |= ADS1115_CFG_MUX_AIN3_G; break;
        default: return -EINVAL;
    }

    /* Set PGA, mode, data rate */
    config &= ~(0x7 << 9);
    config |= ADS1115_CFG_PGA_4V096;   /* ±4.096V for 3.3V FSR circuits */
    config &= ~(0x1 << 8);
    config |= ADS1115_CFG_MODE_SINGLE;  /* Single-shot mode */
    config &= ~(0x7 << 5);
    config |= ADS1115_CFG_DR_860SPS;    /* 860 SPS for fast scanning */
    config |= ADS1115_CFG_OS_START;      /* Start conversion */
    config &= ~0x3;
    config |= ADS1115_CFG_COMP_QUE_1;   /* Disable comparator */

    /* Write config */
    buf[0] = (config >> 8) & 0xFF;
    buf[1] = config & 0xFF;
    ret = i2c_bus_write_reg(ads1115_addr, ADS1115_REG_CONFIG, buf, 2);
    if (ret != 0) return ret;

    /* Wait for conversion (1.2ms at 860 SPS) */
    k_msleep(2);

    /* Poll OS bit until conversion complete */
    int timeout = 100;
    while (timeout--) {
        ret = i2c_bus_read_reg(ads1115_addr, ADS1115_REG_CONFIG, buf, 2);
        if (ret != 0) return ret;
        if (buf[0] & 0x80) break;  /* OS bit set = conversion complete */
        k_msleep(1);
    }
    if (timeout <= 0) {
        LOG_WRN("ADS1115 conversion timeout on channel %d", channel);
        return -ETIMEDOUT;
    }

    /* Read conversion result */
    ret = i2c_bus_read_reg(ads1115_addr, ADS1115_REG_CONV, buf, 2);
    if (ret != 0) return ret;

    *result = (int16_t)((buf[0] << 8) | buf[1]);
    return 0;
}

int ads1115_set_alert_pin(uint16_t lo_threshold, uint16_t hi_threshold)
{
    uint8_t buf[2];

    /* Set lo threshold */
    buf[0] = (lo_threshold >> 8) & 0xFF;
    buf[1] = lo_threshold & 0xFF;
    int ret = i2c_bus_write_reg(ads1115_addr, ADS1115_REG_LO_THRESH, buf, 2);
    if (ret != 0) return ret;

    /* Set hi threshold */
    buf[0] = (hi_threshold >> 8) & 0xFF;
    buf[1] = hi_threshold & 0xFF;
    ret = i2c_bus_write_reg(ads1115_addr, ADS1115_REG_HI_THRESH, buf, 2);
    return ret;
}

float ads1115_to_voltage(int16_t raw, ads1115_gain_t gain)
{
    float lsb_voltage;
    switch (gain) {
        case ADS1115_GAIN_6V144: lsb_voltage = 0.1875e-3f; break;
        case ADS1115_GAIN_4V096: lsb_voltage = 0.125e-3f; break;
        case ADS1115_GAIN_2V048: lsb_voltage = 0.0625e-3f; break;
        case ADS1115_GAIN_1V024: lsb_voltage = 0.03125e-3f; break;
        case ADS1115_GAIN_0V512: lsb_voltage = 0.015625e-3f; break;
        case ADS1115_GAIN_0V256: lsb_voltage = 0.0078125e-3f; break;
        default: lsb_voltage = 0.125e-3f; break;
    }
    return (float)raw * lsb_voltage;
}