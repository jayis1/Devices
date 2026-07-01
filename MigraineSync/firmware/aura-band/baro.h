/**
 * MigraineSync — Aura Band Barometric Header
 * ==========================================
 * License: MIT
 */

#ifndef AURA_BARO_H
#define AURA_BARO_H

/**
 * Initialize BMP390 barometric pressure sensor.
 */
int baro_init(void);

/**
 * Read barometric pressure and temperature.
 * Returns 0 on success.
 */
int baro_read(float *pressure_hpa, float *temp_c);

#endif /* AURA_BARO_H */