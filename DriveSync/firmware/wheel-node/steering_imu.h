#ifndef DRIVESYNC_STEERING_IMU_H
#define DRIVESYNC_STEERING_IMU_H

#include <stdint.h>

/**
 * LSM6DSO 6-axis IMU driver for DriveSync Steering Wheel Node.
 * Samples at 1 kHz with FIFO for low-power operation.
 */

typedef void (*imu_data_cb_t)(int16_t gyro_z, int16_t ax, int16_t ay, int16_t az);

/**
 * Initialize LSM6DSO with FIFO at 1 kHz.
 */
void steering_imu_init(imu_data_cb_t callback);

/**
 * Read FIFO (called internally by timer/interrupt).
 */
void steering_imu_process_fifo(void);

/**
 * Get current steering angular velocity (milli-degrees/sec).
 */
int16_t steering_imu_get_angular_velocity(void);

#endif