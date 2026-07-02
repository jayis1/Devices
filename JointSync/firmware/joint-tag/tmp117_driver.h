/**
 * JointSync Joint Tag — TMP117 Driver Interface
 *
 * License: MIT
 */

#ifndef TMP117_DRIVER_H
#define TMP117_DRIVER_H

#include <stdint.h>
#include "nrf_error.h"

/**
 * Initialize TMP117 temperature sensor.
 */
nrf_err_t tmp117_init(void);

/**
 * Read skin temperature in centi-degrees C (3250 = 32.50°C).
 */
int16_t tmp117_read_temp(void);

/**
 * Read ambient/contralateral temperature (secondary TMP117).
 * Returns 0 if not present.
 */
int16_t tmp117_read_ambient(void);

/**
 * Set temperature alert thresholds.
 */
void tmp117_set_alert(int16_t high_centi, int16_t low_centi);

#endif /* TMP117_DRIVER_H */