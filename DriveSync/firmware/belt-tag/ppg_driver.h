#ifndef DRIVESYNC_PPG_DRIVER_H
#define DRIVESYNC_PPG_DRIVER_H

#include <stdint.h>

/**
 * MAX30101 PPG sensor driver for DriveSync Seat Belt Tag.
 * Reflective photoplethysmography (green + IR LEDs).
 * I2C interface (TWI1: SDA=P0.24, SCL=P0.25).
 */

typedef struct {
    uint16_t ir_samples[8];
    uint16_t red_samples[8];
    uint8_t  hr;          /* bpm, 0=not computed */
    uint8_t  spo2;        /* %, 0=not computed */
    uint8_t  confidence;  /* 0-100 */
} ppg_data_t;

/**
 * Initialize MAX30101 PPG sensor.
 */
void ppg_init(void);

/**
 * Read 8 samples of IR + red LED data.
 */
int ppg_read(ppg_data_t *data);

/**
 * Compute SpO2 from IR/red ratio.
 */
uint8_t ppg_compute_spo2(const uint16_t *ir, const uint16_t *red, uint8_t count);

#endif