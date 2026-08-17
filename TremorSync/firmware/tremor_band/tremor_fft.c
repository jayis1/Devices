/*
 * TremorSync Tremor Band — FFT helpers for tremor frequency analysis
 * 512-point radix-2 Cooley-Tukey FFT + Hann window + band-power computation
 */
#include "tremor_fft.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void tremor_fft_hann_window(float *data, int n)
{
    for (int i = 0; i < n; i++) {
        float w = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (n - 1)));
        data[i] *= w;
    }
}

void tremor_fft_compute(float *re, float *im, int n)
{
    if (n <= 1) return;
    float re_e[256], im_e[256], re_o[256], im_o[256];
    for (int i = 0; i < n / 2; i++) {
        re_e[i] = re[2*i];     im_e[i] = im[2*i];
        re_o[i] = re[2*i + 1]; im_o[i] = im[2*i + 1];
    }
    tremor_fft_compute(re_e, im_e, n/2);
    tremor_fft_compute(re_o, im_o, n/2);
    for (int k = 0; k < n/2; k++) {
        float a = -2.0f * (float)M_PI * k / n;
        float wr = cosf(a), wi = sinf(a);
        float tr = wr*re_o[k] - wi*im_o[k];
        float ti = wr*im_o[k] + wi*re_o[k];
        re[k]       = re_e[k] + tr;
        im[k]       = im_e[k] + ti;
        re[k+n/2]   = re_e[k] - tr;
        im[k+n/2]   = im_e[k] - ti;
    }
}

float tremor_band_power(const float *re, const float *im, int n,
                        float fs, float f_lo, float f_hi)
{
    float bw = fs / n;
    int k_lo = (int)(f_lo / bw);
    int k_hi = (int)(f_hi / bw);
    if (k_hi >= n/2) k_hi = n/2 - 1;
    float pwr = 0.0f;
    for (int k = k_lo; k <= k_hi; k++) {
        pwr += re[k]*re[k] + im[k]*im[k];
    }
    return pwr / (k_hi - k_lo + 1);
}

float tremor_dominant_freq(const float *re, const float *im, int n, float fs)
{
    float max_mag = 0.0f;
    int   max_k   = 0;
    for (int k = 1; k < n/2; k++) {
        float mag = re[k]*re[k] + im[k]*im[k];
        if (mag > max_mag) { max_mag = mag; max_k = k; }
    }
    return max_k * (fs / n);
}