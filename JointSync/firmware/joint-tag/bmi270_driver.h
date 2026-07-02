/**
 * JointSync Joint Tag — BMI270 Driver Interface
 *
 * License: MIT
 */

#ifndef BMI270_DRIVER_H
#define BMI270_DRIVER_H

#include <stdint.h>
#include "nrf_error.h"

typedef struct {
    int16_t accel_x;  /* milli-g */
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;   /* milli-degrees/sec */
    int16_t gyro_y;
    int16_t gyro_z;
} bmi270_data_t;

nrf_err_t bmi270_init(void);
nrf_err_t bmi270_read(bmi270_data_t *data);
void bmi270_config_int1(void (*handler)(void));

#endif /* BMI270_DRIVER_H */