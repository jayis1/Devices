/*
 * stress_detect.c — CalmGrid acute stress episode detection
 *
 * Detects acute stress episodes from concurrent autonomic signals:
 *   - EDA SCR rate spike (sympathetic arousal)
 *   - HRV suppression (parasympathetic withdrawal)
 *   - HR elevation (cardiac acceleration)
 *
 * An acute stress episode requires all three to be sustained for >2 min.
 * This module tracks the sustained-duration requirement using a rolling
 * window of per-minute assessments.
 *
 * Also computes a real-time "stress probability" (0-1) from the
 * autonomic co-activation pattern, used by the hub for early intervention.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include "calm_protocol.h"

#define STRESS_SUSTAIN_MIN   2    /* minutes all conditions must hold */
#define STRESS_HISTORY_LEN   5    /* rolling window (5 min) */

/* Rolling history of per-minute stress conditions */
static uint8_t stress_history[STRESS_HISTORY_LEN];
static uint32_t history_count = 0;
static uint32_t history_head = 0;

/*
 * Check if current vitals meet acute stress criteria for this minute.
 * Returns 1 if all three conditions are met, 0 otherwise.
 */
static int check_stress_conditions(uint16_t cur_scr_rate, uint16_t baseline_scr,
                                    uint16_t cur_hrv, uint16_t baseline_hrv,
                                    uint8_t cur_hr, uint8_t baseline_hr)
{
    if (baseline_scr == 0 || baseline_hrv == 0 || baseline_hr == 0)
        return 0;

    float scr_ratio = (float)cur_scr_rate / (float)baseline_scr;
    float hrv_ratio = (float)cur_hrv / (float)baseline_hrv;
    float hr_ratio  = (float)cur_hr / (float)baseline_hr;

    /* All three must be met */
    return (scr_ratio >= 2.0f && hrv_ratio <= 0.8f && hr_ratio >= 1.1f) ? 1 : 0;
}

/*
 * Detect acute stress episode with sustained-duration requirement.
 * Called once per minute by the wrist band.
 *
 * Returns 1 if a sustained acute stress episode is detected, 0 otherwise.
 */
int detect_acute_stress_episode(uint16_t cur_scr_rate, uint16_t baseline_scr,
                                 uint16_t cur_hrv, uint16_t baseline_hrv,
                                 uint8_t cur_hr, uint8_t baseline_hr)
{
    int conditions_met = check_stress_conditions(cur_scr_rate, baseline_scr,
                                                  cur_hrv, baseline_hrv,
                                                  cur_hr, baseline_hr);

    /* Add to rolling history */
    stress_history[history_head] = conditions_met ? 1 : 0;
    history_head = (history_head + 1) % STRESS_HISTORY_LEN;
    if (history_count < STRESS_HISTORY_LEN) history_count++;

    /* Check if all of the last STRESS_SUSTAIN_MIN minutes met conditions */
    if (history_count < STRESS_SUSTAIN_MIN) return 0;

    for (uint32_t i = 0; i < STRESS_SUSTAIN_MIN; i++) {
        uint32_t idx = (history_head + STRESS_HISTORY_LEN - 1 - i) %
                       STRESS_HISTORY_LEN;
        if (stress_history[idx] == 0) return 0;
    }
    return 1;  /* sustained acute stress detected */
}

/*
 * Compute real-time stress probability (0.0 - 1.0) from autonomic signals.
 * This is a softer, continuous measure — used for early warning before
 * the full episode criteria are met.
 *
 * Uses a weighted combination of normalized deviations:
 *   - EDA SCR rate elevation (weight 0.4)
 *   - HRV suppression (weight 0.35)
 *   - HR elevation (weight 0.25)
 */
float compute_stress_probability(uint16_t cur_scr_rate, uint16_t baseline_scr,
                                  uint16_t cur_hrv, uint16_t baseline_hrv,
                                  uint8_t cur_hr, uint8_t baseline_hr)
{
    if (baseline_scr == 0 || baseline_hrv == 0 || baseline_hr == 0)
        return 0.0f;

    /* EDA: SCR rate ratio, normalized 1x→0, 3x→1 */
    float scr_r = (float)cur_scr_rate / (float)baseline_scr;
    float eda_stress = (scr_r - 1.0f) / 2.0f;
    if (eda_stress < 0) eda_stress = 0;
    if (eda_stress > 1) eda_stress = 1;

    /* HRV: ratio, normalized 1x→0, 0.5x→1 (lower HRV = more stress) */
    float hrv_r = (float)cur_hrv / (float)baseline_hrv;
    float hrv_stress = (1.0f - hrv_r) / 0.5f;
    if (hrv_stress < 0) hrv_stress = 0;
    if (hrv_stress > 1) hrv_stress = 1;

    /* HR: ratio, normalized 1x→0, 1.3x→1 */
    float hr_r = (float)cur_hr / (float)baseline_hr;
    float hr_stress = (hr_r - 1.0f) / 0.3f;
    if (hr_stress < 0) hr_stress = 0;
    if (hr_stress > 1) hr_stress = 1;

    return 0.4f * eda_stress + 0.35f * hrv_stress + 0.25f * hr_stress;
}

/*
 * Reset stress history (e.g., after baseline recalibration).
 */
void stress_detect_reset(void)
{
    memset(stress_history, 0, sizeof(stress_history));
    history_count = 0;
    history_head = 0;
}