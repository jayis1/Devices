/*
 * TremorSync Gait Pod — gait analysis helpers
 * Stride length, cadence, double-support time, festination index
 */
#include "gait_analysis.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979f
#endif

void gait_compute_stride(float *ax, int n, float fs, float *out_length)
{
    /* Peak-to-peak acceleration method for stride length estimation */
    float amin = 1e9f, amax = -1e9f;
    for (int i = 0; i < n; i++) {
        if (ax[i] < amin) amin = ax[i];
        if (ax[i] > amax) amax = ax[i];
    }
    float pp = amax - amin;
    /* Find dominant stride frequency */
    /* (simplified — full FFT done in main.c) */
    float stride_freq = 1.0f;  /* Hz, placeholder */
    *out_length = sqrtf(pp / (stride_freq * stride_freq + 0.01f)) * 0.5f;
}

float gait_cadence_from_steps(int steps, float window_s)
{
    if (window_s <= 0) return 0.0f;
    return (float)steps / window_s * 60.0f;
}

float gait_double_support_ratio(uint16_t *heel, uint16_t *toe, int n,
                                 uint16_t threshold)
{
    int ds_count = 0;
    for (int i = 0; i < n; i++) {
        if (heel[i] > threshold && toe[i] > threshold) ds_count++;
    }
    return (float)ds_count / n;
}

float gait_festination_index(float *stride_history, int n)
{
    /* Compute trend: if stride length is decreasing, index increases */
    if (n < 2) return 0.0f;
    float sum_xy = 0, sum_x = 0, sum_y = 0, sum_x2 = 0;
    for (int i = 0; i < n; i++) {
        sum_x  += i;
        sum_y  += stride_history[i];
        sum_xy += i * stride_history[i];
        sum_x2 += (float)i * i;
    }
    float denom = (float)n * sum_x2 - sum_x * sum_x;
    if (fabsf(denom) < 1e-6f) return 0.0f;
    float slope = ((float)n * sum_xy - sum_x * sum_y) / denom;
    /* Negative slope = decreasing stride = festination */
    return (slope < 0) ? -slope * 100.0f : 0.0f;
}