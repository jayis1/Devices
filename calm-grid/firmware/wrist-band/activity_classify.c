/*
 * activity_classify.c — CalmGrid on-device activity classifier (TFLite Micro)
 *
 * Runs a 1D-CNN on the nRF52840 to classify activity from 2s IMU
 * windows (50Hz × 6 axes = 100 samples × 6 channels).
 *
 * 8 activity classes:
 *   0=sitting, 1=walking, 2=running, 3=resting,
 *   4=sleeping, 5=working(typing), 6=commuting, 7=exercising
 *
 * Model: Conv1D(6→32,k=5) → ReLU → Conv1D(32→32,k=5) → ReLU → MaxPool
 *        → Conv1D(32→16,k=3) → Flatten → Dense(64) → Dense(8) → Softmax
 * Exported as int8 TFLite (<40KB) for nRF52840 TFLite Micro.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "calm_protocol.h"

/* ---- TFLite Micro model embedding (stub) ---- */
const unsigned char activity_model_data[] = {
    0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33,
    0x00, 0x00, 0x00, 0x00,
};
const unsigned int activity_model_len = sizeof(activity_model_data);

/* ---- Heuristic fallback classifier ---- */
/*
 * When the TFLite model is not available, uses signal processing features
 * to classify activity from IMU data:
 *   - Accel magnitude mean + variance → motion intensity
 *   - Gyro magnitude → rotation/movement
 *   - Variance pattern → repetitive vs. sustained motion
 */
static uint8_t heuristic_classify(const int16_t *ax, const int16_t *ay,
                                   const int16_t *az, const int16_t *gx,
                                   const int16_t *gy, const int16_t *gz,
                                   int n)
{
    float accel_mag_mean = 0, accel_mag_var = 0;
    float gyro_mag_mean = 0;
    float accel_mag_min = 1e9, accel_mag_max = 0;

    float mags[100];
    for (int i = 0; i < n; i++) {
        float am = sqrtf((float)ax[i]*ax[i] + ay[i]*ay[i] + az[i]*az[i]);
        float gm = sqrtf((float)gx[i]*gx[i] + gy[i]*gy[i] + gz[i]*gz[i]);
        mags[i] = am;
        accel_mag_mean += am;
        gyro_mag_mean += gm;
        if (am < accel_mag_min) accel_mag_min = am;
        if (am > accel_mag_max) accel_mag_max = am;
    }
    accel_mag_mean /= n;
    gyro_mag_mean /= n;

    for (int i = 0; i < n; i++) {
        float d = mags[i] - accel_mag_mean;
        accel_mag_var += d * d;
    }
    accel_mag_var /= n;
    float accel_std = sqrtf(accel_mag_var);

    /* Gravity ~ 4096 at ±4g (1g = 1024 LSB for ±4g 16-bit → actually 0.061mg/LSB)
     * For this heuristic, use relative thresholds */
    float g = 4096.0f;  /* approximate 1g in raw units */

    /* Classification logic */
    if (accel_std < 50.0f && gyro_mag_mean < 100.0f) {
        /* Very low motion — resting or sleeping */
        if (accel_mag_mean < g * 0.95f || accel_mag_mean > g * 1.05f) {
            /* Tilted (lying down) → sleeping */
            return 4;  /* sleeping */
        }
        return 3;  /* resting */
    }

    if (accel_std < 200.0f && gyro_mag_mean < 500.0f) {
        /* Low motion — sitting or working */
        /* Working has small repetitive motion (typing) */
        if (accel_mag_var > 500.0f && accel_mag_var < 5000.0f)
            return 5;  /* working */
        return 0;  /* sitting */
    }

    if (accel_std < 1000.0f && gyro_mag_mean < 3000.0f) {
        /* Moderate motion — walking or commuting */
        /* Commuting has low-frequency oscillation (vehicle) */
        if (gyro_mag_mean < 1000.0f && accel_mag_var > 200.0f)
            return 6;  /* commuting */
        return 1;  /* walking */
    }

    if (accel_std < 5000.0f) {
        /* High motion — running or exercising */
        if (gyro_mag_mean > 5000.0f)
            return 7;  /* exercising (varied motion) */
        return 2;  /* running */
    }

    return 0;  /* default: sitting */
}

/* ---- Main classification entry point ---- */
uint8_t activity_classify(const int16_t *ax, const int16_t *ay,
                           const int16_t *az, const int16_t *gx,
                           const int16_t *gy, const int16_t *gz, int n)
{
    /* In production: run TFLite Micro inference with activity_model_data
     * For now: use heuristic classifier */
    return heuristic_classify(ax, ay, az, gx, gy, gz, n);
}