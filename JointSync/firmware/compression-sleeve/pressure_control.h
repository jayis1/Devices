/**
 * JointSync Compression Sleeve — Pressure Control Interface
 *
 * License: MIT
 */

#ifndef PRESSURE_CONTROL_H
#define PRESSURE_CONTROL_H

#include <stdint.h>

typedef struct {
    float kp, ki, kd;
    float integral;
    float prev_error;
} pid_t;

void pressure_control_init(void);

/**
 * Read current bladder pressure in mmHg (gauge).
 */
float pressure_control_read(void);

/**
 * Read load cell raw ADC value.
 */
int16_t pressure_control_read_loadcell(void);

/**
 * PID pressure controller.
 * Returns PWM duty cycle [0.0, 1.0].
 */
float pressure_control_pid(float setpoint, float measured);

#endif /* PRESSURE_CONTROL_H */