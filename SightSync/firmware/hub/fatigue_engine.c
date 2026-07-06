/**
 * SightSync Vision Hub — Fatigue Engine Implementation
 *
 * Uses a heuristic fallback when tflite-micro models are not loaded.
 * When models are present (in flash), tflite-micro inference is used.
 *
 * License: MIT
 */

#include "fatigue_engine.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "fatigue";

/* Model loaded flag */
static bool s_models_loaded = false;

/* Heuristic weights for fallback fatigue index computation */
#define W_BLINK        0.25f
#define W_DISTANCE     0.20f
#define W_LUX          0.15f
#define W_BLUE_DOSE    0.10f
#define W_POSTURE      0.15f
#define W_TIME_BREAK   0.15f

void fatigue_engine_init(void)
{
    /* TODO: load tflite-micro models from flash:
     *   - /spiffs/models/fatigue_index.tflite
     *   - /spiffs/models/blink_isoforest.tflite
     * For now, use heuristic fallback.
     */
    s_models_loaded = false;
    ESP_LOGW(TAG, "tflite models not found — using heuristic fallback");
}

uint8_t fatigue_engine_compute(const fatigue_inputs_t *in)
{
    if (s_models_loaded) {
        /* TODO: tflite-micro inference
         *   1. Build input tensor [blink, distance, lux, blue_dose, posture, break_time]
         *   2. Run interpreter
         *   3. Read output (0-100)
         */
    }

    /* Heuristic fallback: weighted normalized scoring */
    float blink_score = 0.0f;
    if (in->blink_rate > 0) {
        /* Normal blink = 15-20 bpm. <8 = high strain. >25 = no strain. */
        if (in->blink_rate < 8) {
            blink_score = 100.0f - (float)in->blink_rate * 10.0f;
        } else if (in->blink_rate < 15) {
            blink_score = 100.0f - (float)in->blink_rate * 5.0f;
        } else {
            blink_score = 0.0f;
        }
        if (blink_score > 100) blink_score = 100;
        if (blink_score < 0) blink_score = 0;
    }

    float distance_score = 0.0f;
    if (in->viewing_distance_mm > 0) {
        /* <300 mm = max strain, >700 mm = no strain */
        if (in->viewing_distance_mm < 300) {
            distance_score = 100.0f;
        } else if (in->viewing_distance_mm < 500) {
            distance_score = (500.0f - (float)in->viewing_distance_mm) / 200.0f * 100.0f;
        } else {
            distance_score = 0.0f;
        }
    }

    float lux_score = 0.0f;
    if (in->ambient_lux > 0) {
        /* <300 lux = insufficient (ISO 8995), >500 = adequate */
        if (in->ambient_lux < 300) {
            lux_score = (300.0f - (float)in->ambient_lux) / 300.0f * 100.0f;
        } else {
            lux_score = 0.0f;
        }
    }

    float blue_score = 0.0f;
    if (in->blue_dose_mj_cm2 > 0) {
        /* >10 mJ/cm² = high strain */
        if (in->blue_dose_mj_cm2 > 10) {
            blue_score = 100.0f;
        } else {
            blue_score = (float)in->blue_dose_mj_cm2 / 10.0f * 100.0f;
        }
    }

    float posture_score = 0.0f;
    if (in->posture_angle_centi != 0) {
        /* >1500 centi-degrees (15°) forward = high strain */
        float angle = fabsf((float)in->posture_angle_centi / 100.0f);
        if (angle > 15.0f) {
            posture_score = (angle - 15.0f) / 30.0f * 100.0f;
            if (posture_score > 100) posture_score = 100;
        }
    }

    float break_score = 0.0f;
    if (in->minutes_since_break > 20) {
        break_score = ((float)in->minutes_since_break - 20.0f) / 40.0f * 100.0f;
        if (break_score > 100) break_score = 100;
    }

    float fatigue = blink_score * W_BLINK +
                    distance_score * W_DISTANCE +
                    lux_score * W_LUX +
                    blue_score * W_BLUE_DOSE +
                    posture_score * W_POSTURE +
                    break_score * W_TIME_BREAK;

    if (fatigue > 100) fatigue = 100;
    if (fatigue < 0) fatigue = 0;

    return (uint8_t)fatigue;
}

uint8_t fatigue_engine_blink_anomaly(uint8_t blink_rate)
{
    /* Simplified isolation forest fallback.
     * If blink_rate < 5 for 2+ consecutive windows → anomaly.
     */
    static uint8_t low_blink_streak = 0;
    if (blink_rate < 5 && blink_rate > 0) {
        low_blink_streak++;
    } else {
        low_blink_streak = 0;
    }

    if (low_blink_streak >= 2) {
        return 1;  /* anomaly */
    }
    return 0;
}

uint8_t fatigue_engine_dry_eye_risk(uint8_t blink_rate, int16_t temp_delta_centi,
                                      uint16_t ambient_lux)
{
    /* Heuristic dry-eye risk:
     *   - blink_rate < 8 bpm → +40
     *   - temp_delta > +50 centi-C (0.5°C above baseline) → +30
     *   - ambient lux < 200 → +15 (low humidity indicator)
     */
    float risk = 0.0f;
    if (blink_rate < 8 && blink_rate > 0) {
        risk += (8.0f - (float)blink_rate) / 8.0f * 40.0f;
    }
    if (temp_delta_centi > 50) {
        risk += 30.0f;
    }
    if (ambient_lux < 200) {
        risk += 15.0f;
    }
    if (risk > 100) risk = 100;
    return (uint8_t)risk;
}