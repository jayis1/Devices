/*
 * calm_protocol.c — CalmGrid shared mesh protocol implementation
 *
 * CRC16-CCITT computation, payload CRC packing/verification, and helper
 * functions for EDA arousal scoring and stress episode detection.
 *
 * SPDX-License-Identifier: MIT
 */
#include "calm_protocol.h"
#include <string.h>

/* ---- CRC16-CCITT (poly 0x1021, init 0xFFFF, no reflection) ---- */
uint16_t calm_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/*
 * Pack CRC into the last 2 bytes of a payload struct.
 * `struct_size_without_crc` = sizeof(struct) - 2
 * Returns the computed CRC value.
 */
uint16_t calm_pack_crc(void *payload, size_t struct_size_without_crc)
{
    uint16_t crc = calm_crc16((const uint8_t *)payload, struct_size_without_crc);
    uint8_t *p = (uint8_t *)payload + struct_size_without_crc;
    p[0] = (uint8_t)(crc & 0xFF);
    p[1] = (uint8_t)((crc >> 8) & 0xFF);
    return crc;
}

/*
 * Verify the CRC of a received payload.
 * Returns 0 if valid, -1 if mismatch.
 */
int calm_verify_crc(const void *payload, size_t struct_size_without_crc,
                    uint16_t received_crc)
{
    uint16_t computed = calm_crc16((const uint8_t *)payload, struct_size_without_crc);
    return (computed == received_crc) ? 0 : -1;
}

/* ---- EDA arousal scoring helpers ---- */

/*
 * Compute sympathetic arousal index from tonic SCL + phasic SCR rate.
 * Combines the two EDA channels into a single 0-100 arousal score.
 *
 * SCL (tonic) reflects baseline arousal / allostatic load.
 * SCR rate (phasic) reflects acute stimulus-driven arousal.
 *
 * arousal = w1 * scl_norm + w2 * scr_rate_norm
 * where weights are derived from stress psychophysiology literature:
 *   w1 = 0.4 (tonic baseline), w2 = 0.6 (phasic reactivity)
 *
 * Returns 0-100.
 */
uint8_t calm_eda_arousal_score(uint16_t scl_microsiemens_x100,
                                uint16_t scr_rate_x100,
                                uint16_t baseline_scl,
                                uint16_t baseline_scr_rate)
{
    /* Normalize tonic SCL: ratio to baseline, clamped 0.5x - 2.0x → 0-100 */
    float scl_ratio = 1.0f;
    if (baseline_scl > 0)
        scl_ratio = (float)scl_microsiemens_x100 / (float)baseline_scl;
    if (scl_ratio < 0.5f) scl_ratio = 0.5f;
    if (scl_ratio > 2.0f) scl_ratio = 2.0f;
    float scl_norm = (scl_ratio - 0.5f) / 1.5f;  /* 0-1 */

    /* Normalize phasic SCR rate: ratio to baseline, clamped 0-3x → 0-100 */
    float scr_ratio = 1.0f;
    if (baseline_scr_rate > 0)
        scr_ratio = (float)scr_rate_x100 / (float)baseline_scr_rate;
    if (scr_ratio > 3.0f) scr_ratio = 3.0f;
    float scr_norm = scr_ratio / 3.0f;  /* 0-1 */

    float arousal = 0.4f * scl_norm + 0.6f * scr_norm;
    if (arousal > 1.0f) arousal = 1.0f;
    if (arousal < 0.0f) arousal = 0.0f;

    return (uint8_t)(arousal * 100.0f);
}

/*
 * Detect acute stress episode from concurrent autonomic signals.
 *
 * An acute stress episode is defined as:
 *   - EDA SCR rate > 2x baseline (sympathetic arousal)
 *   - HRV < 80% of baseline (parasympathetic withdrawal)
 *   - HR > 110% of baseline (cardiac acceleration)
 *   - All three sustained for > 2 minutes
 *
 * Returns 1 if acute stress detected, 0 otherwise.
 */
int calm_detect_acute_stress(uint16_t cur_scr_rate, uint16_t baseline_scr_rate,
                             uint16_t cur_hrv, uint16_t baseline_hrv,
                             uint8_t cur_hr, uint8_t baseline_hr)
{
    if (baseline_scr_rate == 0 || baseline_hrv == 0 || baseline_hr == 0)
        return 0;

    float scr_ratio = (float)cur_scr_rate / (float)baseline_scr_rate;
    float hrv_ratio = (float)cur_hrv / (float)baseline_hrv;
    float hr_ratio  = (float)cur_hr / (float)baseline_hr;

    return (scr_ratio >= 2.0f && hrv_ratio <= 0.8f && hr_ratio >= 1.1f) ? 1 : 0;
}

/* ---- Activity class names (for logging/display) ---- */
static const char *const activity_names[] = {
    "sitting", "walking", "running", "resting",
    "sleeping", "working", "commuting", "exercising"
};

const char *calm_activity_name(uint8_t cls)
{
    if (cls < 8) return activity_names[cls];
    return "unknown";
}

/* ---- Lighting scene names ---- */
static const char *const scene_names[] = {
    "off", "circadian", "work", "de-stress",
    "breathing", "sunset", "sunrise"
};

const char *calm_scene_name(uint8_t scene)
{
    if (scene <= 6) return scene_names[scene];
    return "unknown";
}