/**
 * MigraineSync — Hydrate Tag Load Cell Header
 * ===========================================
 * License: MIT
 */

#ifndef HYDRATE_LOADCELL_H
#define HYDRATE_LOADCELL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize HX711 24-bit load cell ADC.
 */
int loadcell_init(void);

/**
 * Read raw 24-bit value from HX711.
 * Returns raw count, or 0 on error/timeout.
 */
int32_t loadcell_read_raw(void);

/**
 * Read calibrated weight in grams.
 */
float loadcell_read_grams(void);

/**
 * Tare the load cell (set current weight as zero).
 */
void loadcell_tare(void);

/**
 * Power down HX711 (sleep mode).
 */
void loadcell_power_down(void);

/**
 * Power up HX711 from sleep.
 */
void loadcell_power_up(void);

#endif /* HYDRATE_LOADCELL_H */