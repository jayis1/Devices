/*
 * ErgoFlow — LSM6DSOX 6-Axis IMU Driver Header
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef LSM6DSOX_H
#define LSM6DSOX_H

#include <stdint.h>
#include <stdbool.h>

#define LSM6DSOX_I2C_ADDR  0x6A   /* SDO -> GND */

/* Output data rate */
typedef enum {
    LSM6DSOX_ODR_OFF    = 0x00,
    LSM6DSOX_ODR_12HZ  = 0x01,
    LSM6DSOX_ODR_26HZ  = 0x02,
    LSM6DSOX_ODR_52HZ  = 0x03,
    LSM6DSOX_ODR_104HZ = 0x04,
    LSM6DSOX_ODR_208HZ = 0x05,
    LSM6DSOX_ODR_416HZ = 0x06,
    LSM6DSOX_ODR_833HZ = 0x07,
} lsm6dsox_odr_t;

/* Accelerometer data structure (in g) */
typedef struct {
    float x, y, z;
} lsm6dsox_accel_t;

/* Gyroscope data structure (in dps) */
typedef struct {
    float x, y, z;
} lsm6dsox_gyro_t;

/* Initialize LSM6DSOX */
int lsm6dsox_init(uint8_t i2c_addr);

/* Read accelerometer data */
int lsm6dsox_read_accel(lsm6dsox_accel_t *accel);

/* Read gyroscope data */
int lsm6dsox_read_gyro(lsm6dsox_gyro_t *gyro);

/* Read die temperature */
int lsm6dsox_read_temperature(float *temp_c);

/* Get status register */
int lsm6dsox_get_status(uint8_t *status);

/* Check if new accelerometer data is available */
bool lsm6dsox_accel_data_ready(void);

/* Check if new gyroscope data is available */
bool lsm6dsox_gyro_data_ready(void);

/* Set accelerometer ODR */
int lsm6dsox_set_accel_odr(lsm6dsox_odr_t odr);

/* Set gyroscope ODR */
int lsm6dsox_set_gyro_odr(lsm6dsox_odr_t odr);

#endif /* LSM6DSOX_H */