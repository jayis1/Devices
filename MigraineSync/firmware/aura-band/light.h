/**
 * MigraineSync — Aura Band Light Sensor Header
 * ============================================
 * License: MIT
 */

#ifndef AURA_LIGHT_H
#define AURA_LIGHT_H

/**
 * Initialize VEML7700 ambient light sensor.
 */
int light_init(void);

/**
 * Read ambient light level in lux.
 */
int light_read(float *lux);

#endif /* AURA_LIGHT_H */