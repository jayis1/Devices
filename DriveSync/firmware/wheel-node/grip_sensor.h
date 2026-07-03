#ifndef DRIVESYNC_GRIP_SENSOR_H
#define DRIVESYNC_GRIP_SENSOR_H

#include <stdint.h>

/**
 * FDC2214 capacitive grip sensor driver.
 * 4-channel capacitance-to-digital converter.
 * I2C interface (TWI2: SDA=P0.27, SCL=P0.28).
 */

/**
 * Initialize FDC2214.
 */
void grip_sensor_init(void);

/**
 * Read all 4 capacitance channels.
 * @param values Array of 4 uint16_t values (raw capacitance counts).
 */
void grip_sensor_read(uint16_t values[4]);

/**
 * Calibrate baseline (no hands on wheel).
 */
void grip_sensor_calibrate(void);

/**
 * Get hands-on/off state.
 */
uint8_t grip_sensor_get_hands_on(void);

#endif