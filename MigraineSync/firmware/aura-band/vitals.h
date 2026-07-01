/**
 * MigraineSync — Aura Band Vitals Header
 * =======================================
 * License: MIT
 */

#ifndef AURA_VITALS_H
#define AURA_VITALS_H

#include <stdint.h>

typedef struct {
    uint8_t  hr_bpm;        /* heart rate (bpm) */
    float    hrv_rmssd;     /* RMSSD heart rate variability (ms) */
    uint8_t  spo2_pct;      /* SpO₂ (%) */
    float    skin_temp_c;   /* skin temperature (°C) */
    float    skin_temp_slope; /* 6-hour skin temp slope (°C/h) */
    uint8_t  activity;      /* 0=sleep, 1=sedentary, 2=light, 3=moderate, 4=vigorous */
} vitals_t;

/**
 * Initialize MAX30101 PPG sensor.
 */
int vitals_init(void);

/**
 * Read PPG samples and compute HR + HRV.
 * Call every VITALS_INTERVAL_S.
 */
int vitals_read(vitals_t *vitals);

/**
 * Read skin temperature from TMP117.
 */
int skin_temp_read(float *temp_c);

/**
 * Get current activity level from LSM6DSO.
 */
uint8_t activity_get_level(void);

#endif /* AURA_VITALS_H */