/*
 * ErgoFlow — TSL2591 Ambient Light Sensor Driver
 * Used in hub node for circadian lighting control
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "tsl2591.h"
#include "i2c_bus.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>

LOG_MODULE_REGISTER(tsl2591, CONFIG_ERGO_LOG_LEVEL);

#define TSL2591_REG_ENABLE      0x00
#define TSL2591_REG_CONTROL     0x01
#define TSL2591_REG_THRESH_L    0x04
#define TSL2591_REG_THRESH_H    0x05
#define TSL2591_REG_INTERRUPT    0x06
#define TSL2591_REG_CRC         0x08
#define TSL2591_REG_ID          0x0A
#define TSL2591_REG_STATUS      0x13
#define TSL2591_REG_C0DATAL     0x14
#define TSL2591_REG_C0DATAH     0x15
#define TSL2591_REG_C1DATAL     0x16
#define TSL2591_REG_C1DATAH     0x17

#define TSL2591_ID_VAL          0x50

/* Command byte: bit 7=1 (command), bit 6=0 (normal), bit 5=1 (auto-increment) */
#define TSL2591_CMD             0xA0

static uint8_t tsl2591_addr = TSL2591_ADDR;

/* Gain and integration time settings */
static tsl2591_gain_t current_gain = TSL2591_GAIN_MED;
static tsl2591_time_t current_time = TSL2591_TIME_100MS;

int tsl2591_init(uint8_t i2c_addr)
{
    tsl2591_addr = i2c_addr;

    /* Verify device ID */
    uint8_t id = 0;
    int ret = i2c_bus_read_byte(tsl2591_addr, TSL2591_CMD | TSL2591_REG_ID, &id);
    if (ret != 0 || (id & 0xF0) != (TSL2591_ID_VAL & 0xF0)) {
        LOG_ERR("TSL2591 not found (got 0x%02X)", id);
        return -ENODEV;
    }

    /* Enable: power on + ALS enable + interrupt enable */
    ret = i2c_bus_write_byte(tsl2591_addr, TSL2591_CMD | TSL2591_REG_ENABLE, 0x13);
    if (ret != 0) return ret;
    k_msleep(10);

    /* Set gain and integration time */
    ret = tsl2591_set_gain(TSL2591_GAIN_MED);
    ret |= tsl2591_set_time(TSL2591_TIME_100MS);

    LOG_INF("TSL2591 initialized at 0x%02X", tsl2591_addr);
    return ret;
}

int tsl2591_set_gain(tsl2591_gain_t gain)
{
    current_gain = gain;
    uint8_t control = ((uint8_t)current_gain << 4) | (uint8_t)current_time;
    return i2c_bus_write_byte(tsl2591_addr, TSL2591_CMD | TSL2591_REG_CONTROL, control);
}

int tsl2591_set_time(tsl2591_time_t time)
{
    current_time = time;
    uint8_t control = ((uint8_t)current_gain << 4) | (uint8_t)current_time;
    return i2c_bus_write_byte(tsl2591_addr, TSL2591_CMD | TSL2591_REG_CONTROL, control);
}

int tsl2591_read_lux(float *lux)
{
    uint8_t buf[4];
    int ret = i2c_bus_read_reg(tsl2591_addr, TSL2591_CMD | TSL2591_REG_C0DATAL, buf, 4);
    if (ret != 0) return ret;

    uint16_t ch0 = (uint16_t)((buf[1] << 8) | buf[0]);
    uint16_t ch1 = (uint16_t)((buf[3] << 8) | buf[2]);

    /* Check for saturation */
    if (ch0 == 0xFFFF || ch1 == 0xFFFF) {
        *lux = -1.0f;
        return -1;
    }

    /* Calculate lux using TSL2591 formula */
    float gain_factor;
    switch (current_gain) {
        case TSL2591_GAIN_LOW:  gain_factor = 1.0f; break;
        case TSL2591_GAIN_MED:  gain_factor = 25.0f; break;
        case TSL2591_GAIN_HIGH: gain_factor = 428.0f; break;
        case TSL2591_GAIN_MAX:  gain_factor = 9876.0f; break;
        default: gain_factor = 25.0f; break;
    }

    float time_factor;
    switch (current_time) {
        case TSL2591_TIME_100MS: time_factor = 100.0f; break;
        case TSL2591_TIME_200MS: time_factor = 200.0f; break;
        case TSL2591_TIME_300MS: time_factor = 300.0f; break;
        case TSL2591_TIME_400MS: time_factor = 400.0f; break;
        case TSL2591_TIME_500MS: time_factor = 500.0f; break;
        case TSL2591_TIME_600MS: time_factor = 600.0f; break;
        default: time_factor = 100.0f; break;
    }

    float cpl = (gain_factor * time_factor) / 408.0f;
    float lux1 = (ch0 - ch1) * (1.0f - (ch1 / ch0)) / cpl;

    *lux = (lux1 < 0) ? 0 : lux1;
    return 0;
}