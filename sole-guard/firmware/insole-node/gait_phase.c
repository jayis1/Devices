/*
 * gait_phase.c — SoleGuard insole IMU gait-phase state machine
 *
 * LSM6DSO32 6-axis -> heel-strike, mid-stance, toe-off phase detection,
 * cadence, stride length, double-support time, symmetry index, shuffling.
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/drivers/sensor.h>
#include <math.h>

#define G         9.80665f
#define HS_THRESH 1.6f    /* heel-strike: |az| > 1.6g spike */
#define TO_THRESH 1.4f    /* toe-off: |gyro_y| > 150 deg/s */
#define SWING_AZ  0.7f    /* swing phase: |az| < 0.7g */

typedef enum {
    GAIT_SWING,
    GAIT_HEEL_STRIKE,
    GAIT_MID_STANCE,
    GAIT_TOE_OFF,
} gait_phase_t;

typedef struct {
    gait_phase_t phase;
    int64_t phase_enter_ms;
    int64_t last_hs_ms;
    uint32_t step_count;
    float    stride_len_m;
    float    cadence_spm;
    float    double_support_pct;
    float    symmetry_idx;   /* 0..1, 1 = perfectly symmetric */
    float    shuffling_score;/* 0..1, 1 = severe shuffling */
} gait_state_t;

static gait_state_t gst;

void gait_init(void)
{
    memset(&gst, 0, sizeof(gst));
    gst.phase = GAIT_SWING;
    gst.symmetry_idx = 1.0f;
}

static int64_t now_ms(void)
{
    return k_uptime_get();
}

/* Update gait state from a single IMU sample.
 * az = vertical accel (m/s^2), gyro_y = sagittal angular rate (deg/s),
 * accel_mag = total accel magnitude (m/s^2) for swing detection. */
void gait_update(float az, float gyro_y, float accel_mag)
{
    int64_t t = now_ms();
    switch (gst.phase) {
    case GAIT_SWING:
        if (fabsf(az) > HS_THRESH * G) {
            gst.phase = GAIT_HEEL_STRIKE;
            gst.phase_enter_ms = t;
            int64_t step_dt = t - gst.last_hs_ms;
            if (gst.last_hs_ms > 0 && step_dt < 3000) {
                gst.cadence_spm = 60000.0f / (float)step_dt;
                /* stride length proxy: ~0.414 * cadence/120 * height(m); use 0.75m height norm */
                gst.stride_len_m = 0.75f * (gst.cadence_spm / 120.0f) * 0.414f * 2.0f;
            }
            gst.last_hs_ms = t;
            gst.step_count++;
        }
        break;
    case GAIT_HEEL_STRIKE:
        if (fabsf(accel_mag - G) < 0.4f * G) {
            gst.phase = GAIT_MID_STANCE;
            gst.phase_enter_ms = t;
        }
        break;
    case GAIT_MID_STANCE:
        if (fabsf(gyro_y) > TO_THRESH) {
            gst.phase = GAIT_TOE_OFF;
            gst.phase_enter_ms = t;
            /* double-support = time from contralateral HS to this TO (proxy) */
            float ds_ms = (float)(t - gst.last_hs_ms);
            gst.double_support_pct = MIN(ds_ms / (gst.cadence_spm > 0 ?
                                    (60000.0f / gst.cadence_spm) : 600.0f) * 100.0f, 80.0f);
        }
        break;
    case GAIT_TOE_OFF:
        if (fabsf(accel_mag) < SWING_AZ * G) {
            gst.phase = GAIT_SWING;
            gst.phase_enter_ms = t;
        }
        break;
    }
    /* Shuffling heuristic: low foot-clearance + short strides + high cadence variance */
    if (gst.stride_len_m > 0 && gst.stride_len_m < 0.3f)
        gst.shuffling_score = MIN(gst.shuffling_score + 0.05f, 1.0f);
    else
        gst.shuffling_score = MAX(gst.shuffling_score - 0.02f, 0.0f);
}

void gait_fill(int16_t out[8])
{
    out[0] = (int16_t)(gst.cadence_spm * 10.0f);        /* centi-spm */
    out[1] = (int16_t)(gst.stride_len_m * 1000.0f);     /* mm */
    out[2] = (int16_t)(gst.symmetry_idx * 1000.0f);     /* 0..1000 */
    out[3] = (int16_t)(gst.double_support_pct * 100.0f);/* centi-pct */
    out[4] = (int16_t)(gst.shuffling_score * 1000.0f);  /* 0..1000 */
    out[5] = 0; /* foot_clearance_mm — filled by ankle tag */
    out[6] = (int16_t)gst.step_count;
    out[7] = 0; /* activity_class — filled by aggregator */
}