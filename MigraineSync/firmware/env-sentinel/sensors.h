/**
 * MigraineSync — Env Sentinel Sensor Header
 * =========================================
 * License: MIT
 */

#ifndef ENV_SENTINEL_SENSORS_H
#define ENV_SENTINEL_SENSORS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float pressure_hpa;       /* BMP390 barometric pressure */
    float pressure_delta_3h;  /* 3-hour pressure change */
    float light_lux;          /* VEML7700 ambient light */
    float temp_c;             /* SHT45 temperature */
    float humidity_pct;       /* SHT45 humidity */
    float voc_index;          /* BME688 IAQ */
    uint16_t co2_ppm;         /* SCD41 CO₂ */
    uint8_t noise_db;         /* SPL06-007 sound level */
} env_data_t;

/**
 * Initialize all sensors (BMP390, VEML7700, BME688, SCD41, SHT45, SPL06-007)
 * via TCA9548A I²C multiplexer.
 */
int sensors_init(void);

/**
 * Read all sensors into env_data_t.
 * Returns 0 on success, -1 on error.
 */
int sensors_read(env_data_t *data);

/**
 * Update 3-hour pressure delta window.
 * Call every sensor poll.
 */
void sensors_update_pressure_history(float current_pressure);

#endif /* ENV_SENTINEL_SENSORS_H */