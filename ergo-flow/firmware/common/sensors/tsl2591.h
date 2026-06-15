/*
 * ErgoFlow — TSL2591 Ambient Light Sensor Header
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef TSL2591_H
#define TSL2591_H

#include <stdint.h>

#define TSL2591_ADDR  0x29

typedef enum {
    TSL2591_GAIN_LOW  = 0x00,  /* 1x */
    TSL2591_GAIN_MED  = 0x10,  /* 25x */
    TSL2591_GAIN_HIGH = 0x20,  /* 428x */
    TSL2591_GAIN_MAX  = 0x30,  /* 9876x */
} tsl2591_gain_t;

typedef enum {
    TSL2591_TIME_100MS = 0x00,
    TSL2591_TIME_200MS = 0x01,
    TSL2591_TIME_300MS = 0x02,
    TSL2591_TIME_400MS = 0x03,
    TSL2591_TIME_500MS = 0x04,
    TSL2591_TIME_600MS = 0x05,
} tsl2591_time_t;

int tsl2591_init(uint8_t i2c_addr);
int tsl2591_set_gain(tsl2591_gain_t gain);
int tsl2591_set_time(tsl2591_time_t time);
int tsl2591_read_lux(float *lux);

#endif /* TSL2591_H */