/*
 * ambient.c — CalmGrid Room Sentinel ambient environment monitoring
 *
 * Reads ambient light (VEML7700), temperature + humidity (SHT40), and
 * computes noise level from the microphone array. Detects environmental
 * stressors (excessive noise, heat, poor lighting) that compound
 * physiological stress.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <math.h>
#include "calm_protocol.h"

/* Environmental stressor thresholds */
#define NOISE_STRESS_DB      60    /* dB — sustained noise above this is stressful */
#define HEAT_STRESS_C        27    /* °C — above this, thermal discomfort rises */
#define COLD_STRESS_C        18    /* °C — below this, cold discomfort */
#define LOW_LIGHT_LUX        100   /* lux — below this + working = eye strain */
#define HIGH_LIGHT_LUX       2000  /* lux — above this = glare stress */
#define LOW_HUMIDITY_PCT     20    /* % — dry air irritates respiratory system */
#define HIGH_HUMIDITY_PCT    70    /* % — humid air feels oppressive */

/*
 * Compute environmental stress load (0-100) from ambient conditions.
 * Each stressor contributes proportionally to the total load.
 */
uint8_t compute_env_stress_load(uint16_t lux, uint16_t cct,
                                 int16_t temp_centic, uint16_t humidity_centi,
                                 uint16_t noise_db_tenth)
{
    float load = 0;
    float temp_c = temp_centic / 100.0f;
    float hum_pct = humidity_centi / 100.0f;
    float noise_db = noise_db_tenth / 10.0f;

    /* Noise load */
    if (noise_db > NOISE_STRESS_DB) {
        load += (noise_db - NOISE_STRESS_DB) * 2.0f;  /* 2 pts per dB over */
    }

    /* Thermal load */
    if (temp_c > HEAT_STRESS_C) {
        load += (temp_c - HEAT_STRESS_C) * 3.0f;
    } else if (temp_c < COLD_STRESS_C) {
        load += (COLD_STRESS_C - temp_c) * 2.0f;
    }

    /* Light load (too dim or too bright) */
    if (lux < LOW_LIGHT_LUX) {
        load += (LOW_LIGHT_LUX - lux) / 10.0f;
    } else if (lux > HIGH_LIGHT_LUX) {
        load += (lux - HIGH_LIGHT_LUX) / 100.0f;
    }

    /* Humidity load */
    if (hum_pct < LOW_HUMIDITY_PCT) {
        load += (LOW_HUMIDITY_PCT - hum_pct) * 0.5f;
    } else if (hum_pct > HIGH_HUMIDITY_PCT) {
        load += (hum_pct - HIGH_HUMIDITY_PCT) * 0.5f;
    }

    if (load > 100.0f) load = 100.0f;
    if (load < 0.0f) load = 0.0f;
    return (uint8_t)load;
}

/*
 * Detect sudden environmental stress events.
 * Returns flags bitmask for detected events.
 */
uint8_t detect_env_stress_events(uint16_t prev_noise, uint16_t cur_noise,
                                  int16_t prev_temp, int16_t cur_temp,
                                  uint16_t prev_lux, uint16_t cur_lux)
{
    uint8_t flags = 0;

    /* Sudden loud noise (>10 dB jump) */
    if (cur_noise > prev_noise + 100)  /* 10 dB * 10 */
        flags |= CALM_ALERT_ENV_STRESS;

    /* Sudden temperature spike (>2°C) */
    if (cur_temp > prev_temp + 200)  /* 2°C * 100 */
        flags |= CALM_ALERT_ENV_STRESS;

    /* Sudden light flicker (large change) */
    if (prev_lux > 100 && (cur_lux > prev_lux * 3 || cur_lux < prev_lux / 3))
        flags |= CALM_ALERT_ENV_STRESS;

    return flags;
}

/*
 * Suggest a lighting scene based on ambient conditions + activity + stress.
 * Returns a CALM_SCENE_* value.
 */
uint8_t suggest_lighting_scene(uint16_t lux, int16_t temp_centic,
                                uint8_t activity, uint8_t stress_score)
{
    float temp_c = temp_centic / 100.0f;

    /* High stress → de-stress scene always takes priority */
    if (stress_score >= 70)
        return CALM_SCENE_DESTRESS;

    /* Sleeping → off (or very dim warm) */
    if (activity == 4)  /* sleeping */
        return CALM_SCENE_OFF;

    /* Working + dim light → work scene (cool bright) */
    if (activity == 5 && lux < 300)  /* working + dim */
        return CALM_SCENE_WORK;

    /* Evening (low lux naturally) → sunset (warm dimming) */
    if (lux < 50)
        return CALM_SCENE_SUNSET;

    /* Hot environment → cooler CCT (psychological cooling effect) */
    if (temp_c > 27.0f)
        return CALM_SCENE_WORK;  /* cool light */

    /* Default: circadian */
    return CALM_SCENE_CIRCADIAN;
}