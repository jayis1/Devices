/*
 * ErgoFlow — ICM-42688-P 6-Axis IMU Driver Header
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef ICM42688_H
#define ICM42688_H

#include <stdint.h>
#include <stdbool.h>

/* Output data rate */
typedef enum {
    ICM42688_ODR_32000HZ = 0x01,
    ICM42688_ODR_16000HZ = 0x02,
    ICM42688_ODR_8000HZ  = 0x03,
    ICM42688_ODR_4000HZ  = 0x04,
    ICM42688_ODR_2000HZ  = 0x05,
    ICM42688_ODR_1000HZ  = 0x06,
    ICM42688_ODR_200HZ   = 0x07,
    ICM42688_ODR_100HZ   = 0x08,
    ICM42688_ODR_50HZ    = 0x09,
    ICM42688_ODR_25HZ    = 0x0A,
    ICM42688_ODR_12HZ    = 0x0B,
} icm42688_odr_t;

/* Accelerometer data (g) */
typedef struct {
    float x, y, z;
} icm42688_accel_t;

/* Gyroscope data (dps) */
typedef struct {
    float x, y, z;
} icm42688_gyro_t;

/* Initialize ICM-42688 via SPI */
int icm42688_init(void);

/* Read accelerometer data */
int icm42688_read_accel(icm42688_accel_t *accel);

/* Read gyroscope data */
int icm42688_read_gyro(icm42688_gyro_t *gyro);

/* Read die temperature */
int icm42688_read_temp(float *temp_c);

/* Set accelerometer ODR */
int icm42688_set_accel_odr(icm42688_odr_t odr);

/* Set gyroscope ODR */
int icm42688_set_gyro_odr(icm42688_odr_t odr);

#endif /* ICM42688_H */