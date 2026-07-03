#ifndef DRIVESYNC_BODY_IMU_H
#define DRIVESYNC_BODY_IMU_H

#include <stdint.h>

/**
 * LSM6DSO 6-axis IMU driver for DriveSync Seat Belt Tag.
 * Measures body sway (torso oscillation at 0.3-1.5 Hz).
 * I2C interface (TWI2: SDA=P0.27, SCL=P0.28).
 */

/**
 * Initialize LSM6DSO at 50 Hz.
 */
void body_imu_init(void);

/**
 * Read current IMU data.
 * Values in milli-g and milli-degrees/sec.
 */
void body_imu_read(int16_t *ax, int16_t *ay, int16_t *az,
                   int16_t *gx, int16_t *gy, int16_t *gz);

/**
 * Get body sway amplitude (torso oscillation, milli-g).
 */
uint16_t body_imu_get_sway(void);

#endif