/*
 * ppg_hr.c — PawSync PPG heart rate + HRV computation
 *
 * Processes photoplethysmography (PPG) data from the MAX30101 sensor
 * to compute resting heart rate (BPM) and heart rate variability (RMSSD).
 *
 * Algorithm:
 * 1. DC-removal (high-pass filter) to isolate the AC pulsatile component
 * 2. Moving average for smoothing
 * 3. Peak detection (adaptive threshold) to find heartbeats
 * 4. HR = 60 / mean_rr_interval
 * 5. HRV (RMSSD) = sqrt(mean(consecutive_rr_diff²))
 *
 * The MAX30101 IR LED is placed against the pet's neck (in the fur gap)
 * to detect blood volume pulses. For pets with thick fur, the green LED
 * channel can be used alternatively (handled in max30101_init).
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <math.h>
#include "paw_protocol.h"

#define PPG_MAX_PEAKS  64
#define PPG_MIN_HR     40    /* bpm — below this is likely artifact */
#define PPG_MAX_HR     220   /* bpm */
#define PPG_DC_ALPHA   0.95f /* DC removal coefficient */

/* ---- DC removal (high-pass filter) ---- */
static void dc_remove(const uint16_t *input, float *output, int n)
{
    float prev_w = 0.0f;
    for (int i = 0; i < n; i++) {
        float w = input[i] + PPG_DC_ALPHA * prev_w;
        output[i] = w - prev_w;
        prev_w = w;
    }
}

/* ---- Moving average smoothing ---- */
static void moving_average(const float *input, float *output, int n, int window)
{
    int half = window / 2;
    for (int i = 0; i < n; i++) {
        float sum = 0;
        int count = 0;
        for (int j = i - half; j <= i + half; j++) {
            if (j >= 0 && j < n) {
                sum += input[j];
                count++;
            }
        }
        output[i] = sum / count;
    }
}

/* ---- Peak detection (adaptive threshold) ---- */
static int detect_peaks(const float *signal, int n, int *peak_indices, int max_peaks)
{
    int peak_count = 0;
    if (n < 3) return 0;

    /* Compute signal statistics for adaptive threshold */
    float mean = 0, max_val = 0, min_val = 1e9f;
    for (int i = 0; i < n; i++) {
        mean += signal[i];
        if (signal[i] > max_val) max_val = signal[i];
        if (signal[i] < min_val) min_val = signal[i];
    }
    mean /= n;
    float range = max_val - min_val;
    float threshold = mean + range * 0.3f;  /* 30% above mean */

    /* Minimum peak distance: 40bpm = 1.5s → 150 samples @ 100Hz */
    int min_distance = 40;  /* 0.4s @ 100Hz → max 150bpm */

    int last_peak = -min_distance;
    for (int i = 1; i < n - 1; i++) {
        if (signal[i] > threshold &&
            signal[i] > signal[i-1] &&
            signal[i] >= signal[i+1] &&
            (i - last_peak) >= min_distance) {
            if (peak_count < max_peaks)
                peak_indices[peak_count++] = i;
            last_peak = i;
        }
    }
    return peak_count;
}

/* ---- Compute HR and HRV from PPG IR samples ---- */
void ppg_compute_hr_hrv(const uint16_t *ir, int n, uint8_t *hr, uint16_t *hrv)
{
    float filtered[PPG_WINDOW];
    float smoothed[PPG_WINDOW];
    int peaks[PPG_MAX_PEAKS];

    /* Default values */
    *hr = 0;
    *hrv = 0;

    if (n < 100) return;

    /* Step 1: DC removal */
    dc_remove(ir, filtered, n);

    /* Step 2: Smoothing */
    moving_average(filtered, smoothed, n, 5);

    /* Step 3: Peak detection */
    int peak_count = detect_peaks(smoothed, n, peaks, PPG_MAX_PEAKS);

    if (peak_count < 3) {
        /* Not enough peaks — likely poor contact or motion artifact */
        *hr = 0;
        *hrv = 0;
        return;
    }

    /* Step 4: Compute RR intervals (in seconds) */
    float rr_intervals[PPG_MAX_PEAKS - 1];
    int rr_count = 0;
    for (int i = 1; i < peak_count; i++) {
        float dt = (peaks[i] - peaks[i-1]) / 100.0f;  /* 100Hz → seconds */
        if (dt > 0.3f && dt < 2.0f) {  /* 30-200bpm range */
            rr_intervals[rr_count++] = dt;
        }
    }

    if (rr_count < 2) return;

    /* Step 5: HR = 60 / mean(RR) */
    float rr_sum = 0;
    for (int i = 0; i < rr_count; i++)
        rr_sum += rr_intervals[i];
    float mean_rr = rr_sum / rr_count;
    float hr_f = 60.0f / mean_rr;

    /* Sanity check */
    if (hr_f < PPG_MIN_HR || hr_f > PPG_MAX_HR) {
        *hr = 0;
        *hrv = 0;
        return;
    }
    *hr = (uint8_t)(hr_f + 0.5f);

    /* Step 6: HRV (RMSSD) = sqrt(mean(ΔRR²)) */
    float sq_diff_sum = 0;
    for (int i = 1; i < rr_count; i++) {
        float diff = rr_intervals[i] - rr_intervals[i-1];
        sq_diff_sum += diff * diff;
    }
    float rmssd = sqrtf(sq_diff_sum / (rr_count - 1));

    /* Convert to centi-ms (×100) for protocol */
    *hrv = (uint16_t)(rmssd * 1000.0f);  /* ms → centi-ms (×100) */
    /* Note: rmssd in seconds → ×1000 → ms → ×100 → centi-ms */
    /* Actually: rmssd is in seconds. ×1000 = ms. ×100 = centi-ms. So ×100000 total */
    *hrv = (uint16_t)(rmssd * 100000.0f);
}