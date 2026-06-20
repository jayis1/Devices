/*
 * ppg_hrv.c — CalmGrid PPG heart rate + HRV computation
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
 * HRV is the key stress biomarker: parasympathetic (vagal) withdrawal
 * under stress reduces HRV. RMSSD is the time-domain metric most
 * sensitive to vagal tone.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <math.h>
#include "calm_protocol.h"

#define PPG_MAX_PEAKS  64
#define PPG_MIN_HR     40    /* bpm */
#define PPG_MAX_HR     220   /* bpm */
#define PPG_DC_ALPHA   0.95f

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

    /* Compute signal statistics for adaptive threshold */
    float mean = 0, max_val = 0;
    for (int i = 0; i < n; i++) {
        mean += signal[i];
        if (signal[i] > max_val) max_val = signal[i];
    }
    mean /= n;
    float threshold = mean + (max_val - mean) * 0.5f;

    /* Refractory period: min 40 samples @ 100Hz = 400ms → max 150 bpm */
    int refractory = 40;

    for (int i = 2; i < n - 2; i++) {
        if (signal[i] > threshold && signal[i] > signal[i-1] &&
            signal[i] > signal[i+1] && signal[i] > signal[i-2] &&
            signal[i] > signal[i+2]) {
            if (peak_count == 0 ||
                (i - peak_indices[peak_count-1]) >= refractory) {
                peak_indices[peak_count++] = i;
                if (peak_count >= max_peaks) break;
            }
        }
    }
    return peak_count;
}

/* ---- Compute HR + HRV from PPG IR data ---- */
void ppg_compute_hr_hrv(const uint16_t *ir_data, int n,
                        uint8_t *hr, uint16_t *hrv)
{
    if (n < 100) {
        *hr = 0;
        *hrv = 0;
        return;
    }

    float dc_removed[PPG_WINDOW];
    float smoothed[PPG_WINDOW];

    dc_remove(ir_data, dc_removed, n);
    moving_average(dc_removed, smoothed, n, 5);

    int peaks[PPG_MAX_PEAKS];
    int peak_count = detect_peaks(smoothed, n, peaks, PPG_MAX_PEAKS);

    if (peak_count < 3) {
        *hr = 0;
        *hrv = 0;
        return;
    }

    /* Compute RR intervals (in ms @ 100Hz → 10ms per sample) */
    float rr_intervals[PPG_MAX_PEAKS - 1];
    int rr_count = 0;
    for (int i = 1; i < peak_count; i++) {
        float rr = (peaks[i] - peaks[i-1]) * 10.0f;  /* ms */
        /* Filter implausible intervals (30-200 bpm → 300-2000ms) */
        if (rr > 300.0f && rr < 2000.0f) {
            rr_intervals[rr_count++] = rr;
        }
    }

    if (rr_count < 2) {
        *hr = 0;
        *hrv = 0;
        return;
    }

    /* HR = 60000 / mean_rr */
    float mean_rr = 0;
    for (int i = 0; i < rr_count; i++)
        mean_rr += rr_intervals[i];
    mean_rr /= rr_count;

    float hr_f = 60000.0f / mean_rr;
    if (hr_f < PPG_MIN_HR) hr_f = PPG_MIN_HR;
    if (hr_f > PPG_MAX_HR) hr_f = PPG_MAX_HR;
    *hr = (uint8_t)hr_f;

    /* HRV (RMSSD) = sqrt(mean(diff²)) in centi-ms */
    float sum_sq_diff = 0;
    for (int i = 1; i < rr_count; i++) {
        float diff = rr_intervals[i] - rr_intervals[i-1];
        sum_sq_diff += diff * diff;
    }
    float rmssd = sqrtf(sum_sq_diff / (rr_count - 1));
    *hrv = (uint16_t)(rmssd * 100.0f);  /* centi-ms */
}