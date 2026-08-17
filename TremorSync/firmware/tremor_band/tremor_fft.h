#ifndef TREMOR_FFT_H
#define TREMOR_FFT_H

#include <stdint.h>

/* In-place radix-2 Cooley-Tukey FFT (n must be power of 2, max 512) */
void tremor_fft_hann_window(float *data, int n);
void tremor_fft_compute(float *re, float *im, int n);

/* Compute average power in a frequency band [f_lo, f_hi] Hz */
float tremor_band_power(const float *re, const float *im, int n,
                        float fs, float f_lo, float f_hi);

/* Find dominant frequency (Hz) */
float tremor_dominant_freq(const float *re, const float *im, int n, float fs);

#endif /* TREMOR_FFT_H */