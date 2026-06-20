/*
 * eda_gsr.c — CalmGrid Electrodermal Activity (EDA / GSR) processing
 *
 * Processes raw skin conductance data from the AD5940 analog front-end
 * to compute:
 *   - Tonic SCL (Skin Conductance Level) — baseline arousal / allostatic load
 *   - Phasic SCR (Skin Conductance Response) rate — acute arousal events
 *
 * EDA is a direct measure of sympathetic nervous system activation.
 * Unlike heart rate (which has both sympathetic + parasympathetic input),
 * EDA is purely sympathetic — making it the cleanest stress biomarker.
 *
 * Decomposition:
 *   1. Low-pass filter (< 0.05 Hz) → tonic SCL
 *   2. Subtract SCL from raw → phasic SCR (wavelet-like)
 *   3. Detect SCR peaks (threshold + refractory) → count + amplitude
 *   4. SCR rate = peaks per minute
 *
 * The AD5940 applies a 100mV AC excitation at 0.1Hz through Ag/AgCl
 * electrodes on the wrist. The TIA measures conductance (µS).
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <math.h>
#include "calm_protocol.h"

#define EDA_SAMPLE_RATE    4    /* Hz */
#define EDA_WINDOW_SEC     60
#define EDA_WINDOW_SIZE    (EDA_SAMPLE_RATE * EDA_WINDOW_SEC) /* 240 */
#define SCL_LP_CUTOFF_HZ   0.05f
#define SCR_AMP_THRESHOLD  0.5f  /* µS — minimum SCR amplitude */
#define SCR_REFRACTORY_S   3.0f  /* min 3s between SCR peaks */

/* ---- Low-pass filter (simple first-order IIR) ---- */
static void lowpass(const uint16_t *input, float *output, int n, float cutoff_hz,
                    int sample_rate)
{
    float alpha = 1.0f - expf(-2.0f * 3.14159f * cutoff_hz / sample_rate);
    output[0] = (float)input[0] / 100.0f;  /* µS */
    for (int i = 1; i < n; i++) {
        float in = (float)input[i] / 100.0f;
        output[i] = output[i-1] + alpha * (in - output[i-1]);
    }
}

/* ---- SCR peak detection ---- */
static int detect_scr_peaks(const float *phasic, int n, int *peak_idx,
                             float *peak_amp, int max_peaks)
{
    int count = 0;
    int refractory_samples = (int)(SCR_REFRACTORY_S * EDA_SAMPLE_RATE);
    float max_val = 0;

    for (int i = 0; i < n; i++)
        if (phasic[i] > max_val) max_val = phasic[i];

    float threshold = SCR_AMP_THRESHOLD;
    if (max_val < threshold) return 0;  /* no significant SCR */

    for (int i = 2; i < n - 2; i++) {
        if (phasic[i] > threshold && phasic[i] > phasic[i-1] &&
            phasic[i] > phasic[i+1] && phasic[i] > phasic[i-2] &&
            phasic[i] > phasic[i+2]) {
            if (count == 0 || (i - peak_idx[count-1]) >= refractory_samples) {
                peak_idx[count] = i;
                peak_amp[count] = phasic[i];
                count++;
                if (count >= max_peaks) break;
            }
        }
    }
    return count;
}

/* ---- Compute SCL + SCR rate from raw EDA buffer ---- */
void eda_compute_scl_scr(const uint16_t *raw, int n,
                         uint16_t *scl, uint16_t *scr_rate)
{
    if (n < 10) {
        *scl = 0;
        *scr_rate = 0;
        return;
    }

    /* Low-pass for tonic SCL */
    float scl_filtered[EDA_WINDOW_SIZE];
    lowpass(raw, scl_filtered, n, SCL_LP_CUTOFF_HZ, EDA_SAMPLE_RATE);

    /* SCL = median of last 10 samples of filtered signal (stable tonic level) */
    float scl_vals[10];
    int start = n - 10;
    if (start < 0) start = 0;
    for (int i = 0; i < 10 && (start + i) < n; i++)
        scl_vals[i] = scl_filtered[start + i];
    /* Simple median (sort 10 values) */
    for (int i = 0; i < 9; i++)
        for (int j = i + 1; j < 10; j++)
            if (scl_vals[i] > scl_vals[j]) {
                float tmp = scl_vals[i];
                scl_vals[i] = scl_vals[j];
                scl_vals[j] = tmp;
            }
    *scl = (uint16_t)(scl_vals[5] * 100.0f);  /* µS * 100 */

    /* Phasic = raw - SCL (high-pass via subtraction) */
    float phasic[EDA_WINDOW_SIZE];
    for (int i = 0; i < n; i++) {
        float raw_us = (float)raw[i] / 100.0f;
        phasic[i] = raw_us - scl_filtered[i];
    }

    /* Detect SCR peaks */
    int peak_idx[32];
    float peak_amp[32];
    int peak_count = detect_scr_peaks(phasic, n, peak_idx, peak_amp, 32);

    /* SCR rate = peaks per minute * 100 (centi-rate) */
    *scr_rate = (uint16_t)((peak_count * 100 * 60) / EDA_WINDOW_SEC);
}