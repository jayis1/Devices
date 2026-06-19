/*
 * scratch_detect.c — PawSync scratching + head-shaking detection
 *
 * Detects scratching episodes and head-shaking from IMU data using
 * frequency-domain analysis and pattern matching.
 *
 * Scratching signature:
 *   - Sustained high-frequency (8-12Hz) vertical acceleration oscillation
 *   - Low overall body displacement (pet is in one spot)
 *   - Duration: 2-30 seconds per episode
 *   - Frequency: 8-12Hz (much higher than walking ~2Hz)
 *
 * Head-shaking signature:
 *   - Rapid rotational oscillation on yaw axis (3-6Hz)
 *   - High amplitude (±1000°/s or more)
 *   - Duration: 0.5-3 seconds per shake
 *   - Often repeated in clusters (ear infection sign)
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <math.h>
#include "paw_protocol.h"

#define SAMPLE_HZ       50
#define WINDOW_SAMPLES  100   /* 2s @ 50Hz */
#define SCRATCH_FREQ_MIN 8    /* Hz */
#define SCRATCH_FREQ_MAX 12   /* Hz */
#define HEADSHAKE_FREQ_MIN 3  /* Hz */
#define HEADSHAKE_FREQ_MAX 6  /* Hz */
#define SCRATCH_AMP_MIN  200  /* min accel amplitude (raw LSBs) */
#define HEADSHAKE_AMP_MIN 800 /* min gyro amplitude (mdps raw) */

/* ---- Zero-crossing frequency estimation ---- */
static float estimate_dominant_freq(const int16_t *signal, int n, int sample_hz)
{
    if (n < 10) return 0;

    /* Count zero crossings */
    int crossings = 0;
    for (int i = 1; i < n; i++) {
        if ((signal[i] > 0 && signal[i-1] <= 0) ||
            (signal[i] < 0 && signal[i-1] >= 0))
            crossings++;
    }
    /* Frequency = crossings / 2 / duration */
    float duration = (float)n / sample_hz;
    return (float)crossings / 2.0f / duration;
}

/* ---- Compute RMS amplitude ---- */
static float compute_rms(const int16_t *signal, int n)
{
    float sum_sq = 0;
    for (int i = 0; i < n; i++)
        sum_sq += (float)signal[i] * signal[i];
    return sqrtf(sum_sq / n);
}

/* ---- Detect scratching from vertical accel ---- */
int scratch_detect_from_imu(const int16_t *az, int n)
{
    if (n < 30) return 0;

    /* Check amplitude */
    float rms = compute_rms(az, n);
    if (rms < SCRATCH_AMP_MIN) return 0;

    /* Check frequency: scratching is 8-12Hz */
    float freq = estimate_dominant_freq(az, n, SAMPLE_HZ);
    if (freq >= SCRATCH_FREQ_MIN && freq <= SCRATCH_FREQ_MAX)
        return 1;

    /* Also check for high zero-crossing rate (proxy for high freq) */
    int crossings = 0;
    for (int i = 1; i < n; i++) {
        if ((az[i] > 0 && az[i-1] <= 0) || (az[i] < 0 && az[i-1] >= 0))
            crossings++;
    }
    /* 8Hz @ 50Hz × 2s = 400 samples → ~16 crossings per Hz per 2s */
    /* 8Hz → ~16 crossings, 12Hz → ~24 crossings in 2s window */
    if (crossings >= 14 && rms > SCRATCH_AMP_MIN)
        return 1;

    return 0;
}

/* ---- Detect head-shaking from yaw gyro ---- */
int head_shake_detect_from_imu(const int16_t *gz, int n)
{
    if (n < 30) return 0;

    /* Check amplitude */
    float rms = compute_rms(gz, n);
    if (rms < HEADSHAKE_AMP_MIN) return 0;

    /* Check frequency: head-shaking is 3-6Hz */
    float freq = estimate_dominant_freq(gz, n, SAMPLE_HZ);
    if (freq >= HEADSHAKE_FREQ_MIN && freq <= HEADSHAKE_FREQ_MAX)
        return 1;

    /* Check for rapid direction changes */
    int direction_changes = 0;
    int16_t prev = gz[0];
    for (int i = 1; i < n; i++) {
        if (prev > HEADSHAKE_AMP_MIN && gz[i] < -HEADSHAKE_AMP_MIN)
            direction_changes++;
        else if (prev < -HEADSHAKE_AMP_MIN && gz[i] > HEADSHAKE_AMP_MIN)
            direction_changes++;
        prev = gz[i];
    }
    /* 3-6Hz → expect 6-12 direction changes in 2s */
    if (direction_changes >= 5 && rms > HEADSHAKE_AMP_MIN)
        return 1;

    return 0;
}