/**
 * JointSync Compression Sleeve — Pump Driver Interface
 *
 * License: MIT
 */

#ifndef PUMP_DRIVER_H
#define PUMP_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

void pump_driver_init(void);
void pump_driver_inflate(void);
void pump_driver_deflate(void);
void pump_driver_hold(void);
void pump_driver_off(void);
void pump_driver_set_pwm(float duty);
bool pump_driver_check_fault(void);

#endif /* PUMP_DRIVER_H */