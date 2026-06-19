/*
 * activity_cnn.c — PawSync on-device activity classifier (TFLite Micro)
 *
 * Runs a 1D-CNN on the nRF52840 to classify pet activity from 2s IMU
 * windows (50Hz × 6 axes = 100 samples × 6 channels).
 *
 * 9 activity classes:
 *   0=resting, 1=walking, 2=running, 3=sleeping,
 *   4=scratching, 5=head_shaking, 6=licking, 7=eating, 8=playing
 *
 * Model: Conv1D(6→32,k=5) → ReLU → Conv1D(32→32,k=5) → ReLU → MaxPool
 *        → Conv1D(32→16,k=3) → Flatten → Dense(64) → Dense(9) → Softmax
 * Exported as int8 TFLite (<40KB) for nRF52840 TFLite Micro.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "paw_protocol.h"

/* ---- TFLite Micro model embedding (stub) ---- */
const unsigned char activity_model_data[] = {
    0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33,
    0x00, 0x00, 0x00, 0x00,
};
const unsigned int activity_model_len = sizeof(activity_model_data);

/* ---- Heuristic fallback classifier ---- */
/*
 * When the TFLite model is not available, uses signal processing features
 * to classify activity. Not as accurate as the CNN but sufficient for
 * basic activity tracking.
 */
static uint8_t heuristic_classify(const int16_t *ax, const int16_t *ay,
                                   const int16_t *az, const int16_t *gx,
                                   const int16_t *gy, const int16_t *gz,
                                   int n)
{
    /* Compute signal features */
    float accel_mag_mean = 0, accel_mag_var = 0;
    float gyro_mag_mean = 0;

    float mags[100];
    for (int i = 0; i < n; i++) {
        float am = sqrtf((float)ax[i]*ax[i] + ay[i]*ay[i] + az[i]*az[i]);
        float gm = sqrtf((float)gx[i]*gx[i] + gy[i]*gy[i] + gz[i]*gz[i]);
        mags[i] = am;
        accel_mag_mean += am;
        gyro_mag_mean += gm;
    }
    accel_mag_mean /= n;
    gyro_mag_mean /= n;

    for (int i = 0; i < n; i++) {
        float d = mags[i] - accel_mag_mean;
        accel_mag_var += d * d;
    }
    accel_mag_var /= n;

    /* Classification rules:
     *   Very low variance + low gyro → sleeping
     *   Low variance + low gyro → resting
     *   Moderate variance + moderate gyro → walking
     *   High variance + high gyro → running
     *   High-frequency vertical accel → scratching
     *   High rotational gyro (yaw) → head shaking
     *   Low-amplitude periodic → licking
     */
    float accel_std = sqrtf(accel_mag_var);

    /* Check for scratching: high-frequency content in vertical (Z) axis */
    int z_crossings = 0;
    for (int i = 1; i < n; i++) {
        if ((az[i] > 200 && az[i-1] < -200) || (az[i] < -200 && az[i-1] > 200))
            z_crossings++;
    }
    if (z_crossings > 8)
        return 4;  /* scratching */

    /* Check for head shaking: high yaw rotational velocity */
    int yaw_crossings = 0;
    for (int i = 1; i < n; i++) {
        if ((gz[i] > 500 && gz[i-1] < -500) || (gz[i] < -500 && gz[i-1] > 500))
            yaw_crossings++;
    }
    if (yaw_crossings > 4)
        return 5;  /* head shaking */

    /* Activity level classification */
    if (accel_std < 20 && gyro_mag_mean < 50)
        return 3;  /* sleeping */
    if (accel_std < 50 && gyro_mag_mean < 100)
        return 0;  /* resting */
    if (accel_std < 150 && gyro_mag_mean < 500)
        return 1;  /* walking */
    if (accel_std < 400 && gyro_mag_mean < 1500)
        return 6;  /* licking (low periodic) */
    if (accel_std >= 400 || gyro_mag_mean >= 1500)
        return 2;  /* running */
    if (accel_std > 200 && gyro_mag_mean > 800)
        return 8;  /* playing */

    return 0;  /* default: resting */
}

static uint8_t compute_confidence(float accel_std, float gyro_mean)
{
    /* Higher signal-to-noise → higher confidence */
    uint8_t conf = 60;
    if (accel_std > 100) conf += 15;
    if (gyro_mean > 500) conf += 15;
    if (conf > 95) conf = 95;
    return conf;
}

/* ---- Public API: classify activity from 2s IMU window ---- */
void activity_classify(const int16_t *ax, const int16_t *ay,
                       const int16_t *az, const int16_t *gx,
                       const int16_t *gy, const int16_t *gz,
                       int n, uint8_t *class_out, uint8_t *conf_out)
{
    /* In production: invoke TFLite Micro model here.
     * For now: use heuristic classifier. */
    uint8_t cls = heuristic_classify(ax, ay, az, gx, gy, gz, n);

    /* Compute confidence */
    float accel_mag_mean = 0;
    for (int i = 0; i < n; i++)
        accel_mag_mean += sqrtf((float)ax[i]*ax[i] + ay[i]*ay[i] + az[i]*az[i]);
    accel_mag_mean /= n;
    float gyro_mean = 0;
    for (int i = 0; i < n; i++)
        gyro_mean += sqrtf((float)gx[i]*gx[i] + gy[i]*gy[i] + gz[i]*gz[i]);
    gyro_mean /= n;
    float accel_std = 0;
    for (int i = 0; i < n; i++) {
        float am = sqrtf((float)ax[i]*ax[i] + ay[i]*ay[i] + az[i]*az[i]);
        float d = am - accel_mag_mean;
        accel_std += d * d;
    }
    accel_std = sqrtf(accel_std / n);

    *class_out = cls;
    *conf_out = compute_confidence(accel_std, gyro_mean);
}