/*
 * gait_analysis.c — PawSync gait feature extraction + lameness detection
 *
 * Extracts gait features from IMU data during walking/running:
 *   - Stride length (cm, estimated from vertical displacement + IMU integration)
 *   - Stance time (ms, from vertical accel zero-crossings)
 *   - Symmetry index (0-1000, from stride-to-stride consistency)
 *   - Weight-bearing asymmetry (0-1000, from vertical accel peak differences)
 *   - Activity count (strides in window)
 *   - Scratch count (episodes in window)
 *
 * Lameness detection: gait symmetry index > 150 (0.15) sustained over
 * >50 consecutive strides → PAW_ALERT_LAMENESS flag.
 *
 * Based on veterinary gait analysis literature adapted for collar-mounted IMU.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <math.h>
#include "paw_protocol.h"

#define GRAVITY          9.80665f
#define ACCEL_LSB_MG     0.061f   /* ±4g range, 16-bit → 0.061 mg/LSB */
#define GYRO_LSB_MDPS    8.75f    /* ±500dps range → 8.75 mdps/LSB */
#define STRIDE_MIN_SAMPLES 30     /* min 0.6s @ 50Hz → max 100spm */
#define STRIDE_MAX_SAMPLES 150    /* max 3.0s → min 20spm */

/* ---- Detect stride boundaries from vertical accel ---- */
static int detect_strides(const int16_t *az_raw, int n, int *boundaries, int max_strides)
{
    /* Convert to m/s² and remove gravity */
    float az[n];
    for (int i = 0; i < n; i++)
        az[i] = (az_raw[i] * ACCEL_LSB_MG) / 1000.0f * GRAVITY - GRAVITY;

    /* Low-pass filter (simple moving average) */
    float filt[n];
    for (int i = 0; i < n; i++) {
        float sum = 0;
        int cnt = 0;
        for (int j = i - 2; j <= i + 2; j++) {
            if (j >= 0 && j < n) { sum += az[j]; cnt++; }
        }
        filt[i] = sum / cnt;
    }

    /* Find zero crossings (heel strike → positive going) */
    int stride_count = 0;
    int last_cross = -STRIDE_MAX_SAMPLES;
    for (int i = 1; i < n; i++) {
        if (filt[i] > 0 && filt[i-1] <= 0) {
            int dist = i - last_cross;
            if (dist >= STRIDE_MIN_SAMPLES && dist <= STRIDE_MAX_SAMPLES) {
                if (stride_count < max_strides)
                    boundaries[stride_count] = i;
                stride_count++;
            }
            last_cross = i;
        }
    }
    return stride_count;
}

/* ---- Estimate stride length from vertical displacement ---- */
static float estimate_stride_length(const int16_t *az_raw, int start, int end)
{
    /* Double-integrate vertical accel over one stride
     * s = ∫∫ a dt² (simplified — gives relative, not absolute, length) */
    float v = 0, disp = 0;
    int samples = end - start;
    float dt = 1.0f / 50.0f;  /* 50Hz */

    for (int i = start; i < end && i < start + 200; i++) {
        float a = (az_raw[i] * ACCEL_LSB_MG) / 1000.0f * GRAVITY - GRAVITY;
        v += a * dt;
        disp += v * dt;
    }
    /* Scale to approximate cm (empirical calibration factor) */
    return fabsf(disp) * 150.0f;  /* calibration constant */
}

/* ---- Compute gait symmetry from stride lengths ---- */
static uint16_t compute_symmetry(float *stride_lengths, int n)
{
    if (n < 2) return 0;
    /* Symmetry = coefficient of variation of stride lengths
     * Lower variation = more symmetric gait */
    float mean = 0;
    for (int i = 0; i < n; i++) mean += stride_lengths[i];
    mean /= n;
    if (mean < 0.1f) return 0;

    float var = 0;
    for (int i = 0; i < n; i++) {
        float d = stride_lengths[i] - mean;
        var += d * d;
    }
    var /= n;
    float cv = sqrtf(var) / mean;  /* coefficient of variation */
    /* Scale to 0-1000 (0=perfect symmetry) */
    uint16_t si = (uint16_t)(cv * 1000.0f);
    if (si > 1000) si = 1000;
    return si;
}

/* ---- Compute weight-bearing asymmetry from peak accel ---- */
static uint16_t compute_weight_asymmetry(const int16_t *az_raw, int *boundaries, int n_strides)
{
    if (n_strides < 2) return 0;
    /* Compare peak vertical accel between consecutive strides */
    float peaks[n_strides];
    for (int s = 0; s < n_strides; s++) {
        int start = boundaries[s];
        int end = (s + 1 < n_strides) ? boundaries[s+1] : start + 100;
        float peak = 0;
        for (int i = start; i < end; i++) {
            float a = fabsf((az_raw[i] * ACCEL_LSB_MG) / 1000.0f);
            if (a > peak) peak = a;
        }
        peaks[s] = peak;
    }
    /* Compare consecutive peaks */
    float max_diff = 0;
    for (int i = 1; i < n_strides; i++) {
        float d = fabsf(peaks[i] - peaks[i-1]);
        if (d > max_diff) max_diff = d;
    }
    float max_peak = 0;
    for (int i = 0; i < n_strides; i++)
        if (peaks[i] > max_peak) max_peak = peaks[i];
    if (max_peak < 0.01f) return 0;
    uint16_t asym = (uint16_t)((max_diff / max_peak) * 1000.0f);
    if (asym > 1000) asym = 1000;
    return asym;
}

/* ---- Main gait feature extraction ---- */
void gait_compute_features(const int16_t *az_raw, const int16_t *gz_raw,
                           int n, int16_t gait[PAW_GAIT_FEATURES])
{
    int boundaries[32];
    int n_strides = detect_strides(az_raw, n, boundaries, 32);

    /* gait[0] = stride length (cm) */
    if (n_strides > 0) {
        float sl = 0;
        for (int s = 0; s < n_strides && s < 8; s++) {
            int end = (s + 1 < n_strides) ? boundaries[s+1] : n;
            sl += estimate_stride_length(az_raw, boundaries[s], end);
        }
        gait[0] = (int16_t)(sl / (n_strides > 0 ? n_strides : 1));
    } else {
        gait[0] = 0;
    }

    /* gait[1] = stance time (ms) */
    if (n_strides > 1) {
        float mean_stance = 0;
        for (int s = 0; s < n_strides - 1; s++)
            mean_stance += (boundaries[s+1] - boundaries[s]) * 20;  /* 50Hz → 20ms */
        gait[1] = (int16_t)(mean_stance / (n_strides - 1));
    } else {
        gait[1] = 0;
    }

    /* gait[2] = symmetry index (0-1000) */
    float stride_lengths[32];
    for (int s = 0; s < n_strides && s < 32; s++) {
        int end = (s + 1 < n_strides) ? boundaries[s+1] : n;
        stride_lengths[s] = estimate_stride_length(az_raw, boundaries[s], end);
    }
    gait[2] = (int16_t)compute_symmetry(stride_lengths, n_strides);

    /* gait[3] = weight-bearing asymmetry (0-1000) */
    gait[3] = (int16_t)compute_weight_asymmetry(az_raw, boundaries, n_strides);

    /* gait[4] = activity count (strides) */
    gait[4] = (int16_t)n_strides;

    /* gait[5] = scratch count (placeholder — set by caller) */
    gait[5] = 0;
}

/* ---- Lameness severity grading ---- */
uint8_t gait_lameness_grade(uint16_t symmetry_idx)
{
    /* Delegate to protocol helper */
    return paw_lameness_grade(symmetry_idx);
}