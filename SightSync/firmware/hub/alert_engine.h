/**
 * SightSync Vision Hub — Alert Engine
 *
 * Evaluates fatigue, distance, dry-eye thresholds and triggers
 * audio + haptic + visual alerts.
 *
 * License: MIT
 */

#ifndef ALERT_ENGINE_H
#define ALERT_ENGINE_H

#include <stdint.h>

typedef struct {
    uint8_t  fatigue_score;
    uint8_t  blink_rate_bpm;
    uint16_t viewing_distance_mm;
    uint32_t near_work_minutes_today;
    uint16_t ambient_lux;
    uint8_t  forward_head_flag;
    uint8_t  posture_risk;
    int16_t  periocular_temp_delta;
    uint8_t  dry_eye_risk;
    uint32_t minutes_since_break;
    uint8_t  break_overdue;
    uint8_t  mode;
} hub_state_t;

void alert_engine_init(void);
void alert_engine_evaluate(hub_state_t *state);
void alert_engine_break_reminder(void);

#endif /* ALERT_ENGINE_H */