/*
 * TremorSync Voice Node — speech feature extraction helpers
 * RMS, F0 (autocorrelation pitch), jitter, shimmer, HNR, formant slopes
 */
#include "speech_features.h"
#include <math.h>

void speech_compute_rms(const int16_t *s, int n, float *rms)
{
    float sum = 0;
    for (int i = 0; i < n; i++) {
        float v = (float)s[i] / 32768.0f;
        sum += v * v;
    }
    *rms = sqrtf(sum / n);
}

float speech_detect_f0(const int16_t *s, int n, float fs)
{
    int min_lag = (int)(fs / 400.0f);
    int max_lag = (int)(fs / 60.0f);
    if (max_lag >= n) max_lag = n - 1;
    float best_corr = 0, best_lag = 0;
    for (int lag = min_lag; lag < max_lag; lag++) {
        float c = 0;
        for (int i = 0; i < n - lag; i++) {
            float a = (float)s[i] / 32768.0f;
            float b = (float)s[i + lag] / 32768.0f;
            c += a * b;
        }
        c /= (n - lag);
        if (c > best_corr) { best_corr = c; best_lag = (float)lag; }
    }
    return (best_lag > 0) ? fs / best_lag : 0.0f;
}

void speech_compute_jitter(const int16_t *s, int n, float fs, float *jitter)
{
    /* Jitter = average absolute difference between consecutive periods / mean period */
    /* Extract periods via zero-crossing */
    int periods[64];
    int pcount = 0;
    int prev_zero = -1;
    for (int i = 1; i < n && pcount < 64; i++) {
        if (s[i-1] < 0 && s[i] >= 0) {
            if (prev_zero >= 0) {
                periods[pcount++] = i - prev_zero;
            }
            prev_zero = i;
        }
    }
    if (pcount < 2) { *jitter = 0; return; }
    float sum_diff = 0, sum_p = 0;
    for (int i = 0; i < pcount; i++) {
        sum_p += periods[i];
        if (i > 0) sum_diff += fabsf((float)periods[i] - periods[i-1]);
    }
    float mean_p = sum_p / pcount;
    *jitter = (sum_diff / (pcount - 1)) / mean_p * 100.0f;
}

void speech_compute_shimmer(const int16_t *s, int n, float *shimmer)
{
    /* Shimmer = average absolute difference between consecutive peak amplitudes / mean amplitude */
    int peaks[64];
    int pcount = 0;
    for (int i = 2; i < n - 2 && pcount < 64; i++) {
        if (s[i] > s[i-1] && s[i] > s[i+1] && s[i] > s[i-2] && s[i] > s[i+2]) {
            peaks[pcount++] = abs(s[i]);
        }
    }
    if (pcount < 2) { *shimmer = 0; return; }
    float sum_diff = 0, sum_a = 0;
    for (int i = 0; i < pcount; i++) {
        sum_a += peaks[i];
        if (i > 0) sum_diff += fabsf((float)peaks[i] - peaks[i-1]);
    }
    float mean_a = sum_a / pcount;
    *shimmer = (sum_diff / (pcount - 1)) / mean_a * 100.0f;
}

float speech_hnr(const int16_t *s, int n)
{
    /* Harmonics-to-noise ratio via autocorrelation */
    /* HNR = 10 * log10(R_max / (1 - R_max)) where R_max is peak normalized autocorr */
    float best_r = 0;
    int min_lag = 40, max_lag = 400;
    if (max_lag >= n) max_lag = n / 2;
    for (int lag = min_lag; lag < max_lag; lag++) {
        float c = 0, energy = 0;
        for (int i = 0; i < n - lag; i++) {
            float a = (float)s[i] / 32768.0f;
            float b = (float)s[i + lag] / 32768.0f;
            c += a * b;
            energy += a * a;
        }
        if (energy > 0) {
            float r = c / energy;
            if (r > best_r) best_r = r;
        }
    }
    if (best_r >= 0.999f) best_r = 0.998f;
    if (best_r <= 0) return 0;
    return 10.0f * log10f(best_r / (1.0f - best_r));
}