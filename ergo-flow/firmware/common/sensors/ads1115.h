/*
 * ErgoFlow — ADS1115 16-bit ADC Driver Header
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef ADS1115_H
#define ADS1115_H

#include <stdint.h>

/* I2C addresses */
#define ADS1115_ADDR_GND  0x48   /* ADR -> GND */
#define ADS1115_ADDR_VDD  0x49   /* ADR -> VDD */
#define ADS1115_ADDR_SDA  0x4A   /* ADR -> SDA */
#define ADS1115_ADDR_SCL  0x4B   /* ADR -> SCL */

/* Gain settings */
typedef enum {
    ADS1115_GAIN_6V144 = 0,   /* ±6.144V */
    ADS1115_GAIN_4V096 = 1,   /* ±4.096V */
    ADS1115_GAIN_2V048 = 2,   /* ±2.048V */
    ADS1115_GAIN_1V024 = 3,   /* ±1.024V */
    ADS1115_GAIN_0V512 = 4,   /* ±0.512V */
    ADS1115_GAIN_0V256 = 5,   /* ±0.256V */
} ads1115_gain_t;

/* Initialize ADS1115 at given I2C address */
int ads1115_init(uint8_t i2c_addr);

/* Read a single-ended channel (0-3) */
int ads1115_read_channel(uint8_t channel, int16_t *result);

/* Configure alert pin thresholds */
int ads1115_set_alert_pin(uint16_t lo_threshold, uint16_t hi_threshold);

/* Convert raw ADC value to voltage */
float ads1115_to_voltage(int16_t raw, ads1115_gain_t gain);

#endif /* ADS1115_H */