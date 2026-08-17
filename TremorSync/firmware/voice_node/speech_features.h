#ifndef SPEECH_FEATURES_H
#define SPEECH_FEATURES_H

#include <stdint.h>

void  speech_compute_rms(const int16_t *s, int n, float *rms);
float speech_detect_f0(const int16_t *s, int n, float fs);
void  speech_compute_jitter(const int16_t *s, int n, float fs, float *jitter);
void  speech_compute_shimmer(const int16_t *s, int n, float *shimmer);
float speech_hnr(const int16_t *s, int n);

#endif