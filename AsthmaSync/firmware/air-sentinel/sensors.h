/**
 * AsthmaSync — Air Sentinel Sensor Header
 *
 * License: MIT
 */

#ifndef SENSORS_H
#define SENSORS_H

#include "../common/protocol.h"
#include <stdint.h>

/* PMSA003I data */
typedef struct {
    uint16_t pm1_0_ug;   /* PM1.0 standard (µg/m³) */
    uint16_t pm2_5_ug;
    uint16_t pm10_ug;
    uint16_t pm1_0_raw;  /* PM1.0 under atmospheric env */
    uint16_t pm2_5_raw;
    uint16_t pm10_raw;
} pmsa_data_t;

/* BME688 data */
typedef struct {
    float temperature_c;
    float pressure_hpa;
    float humidity_pct;
    uint16_t voc_index;     /* 0-500 IAQ */
    uint32_t gas_resistance_ohm;
} bme688_data_t;

/* SCD41 data */
typedef struct {
    uint16_t co2_ppm;      /* 400-5000 ppm */
    float temperature_c;
    float humidity_pct;
} scd41_data_t;

/* Driver functions */
int pmsa003i_init(void);
int pmsa003i_read(pmsa_data_t *out);

int bme688_init(void);
int bme688_read(bme688_data_t *out);

int sgp40_init(void);
int sgp40_read(uint16_t *voc_index);

int scd41_init(void);
int scd41_read(scd41_data_t *out);

/** Read all sensors and pack into protocol air_quality_t struct. */
int sensors_pack_air_quality(air_quality_t *out);

#endif /* SENSORS_H */