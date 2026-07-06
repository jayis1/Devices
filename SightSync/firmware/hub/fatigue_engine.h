/**
 * SightSync Vision Hub — Fatigue Engine
 *
 * Computes Visual Fatigue Index (XGBoost → tflite-micro),
 * blink anomaly (isolation forest → tflite), and dry-eye risk.
 *
 * License: MIT
 */

#ifndef FATIGUE_ENGINE_H
#define FATIGUE_ENGINE_H

#include <stdint.h>

typedef struct {
    uint8_t  blink_rate;
    uint16_t viewing_distance_mm;
    uint16_t ambient_lux;
    uint16_t blue_dose_mj_cm2;
    int16_t  posture_angle_centi;
    uint32_t minutes_since_break;
} fatigue_inputs_t;

void     fatigue_engine_init(void);
uint8_t  fatigue_engine_compute(const fatigue_inputs_t *inputs);
uint8_t  fatigue_engine_blink_anomaly(uint8_t blink_rate);
uint8_t  fatigue_engine_dry_eye_risk(uint8_t blink_rate, int16_t temp_delta_centi,
                                       uint16_t ambient_lux);

#endif /* FATIGUE_ENGINE_H */