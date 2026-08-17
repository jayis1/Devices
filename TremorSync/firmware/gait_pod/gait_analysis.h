#ifndef GAIT_ANALYSIS_H
#define GAIT_ANALYSIS_H

#include <stdint.h>

void  gait_compute_stride(float *ax, int n, float fs, float *out_length);
float gait_cadence_from_steps(int steps, float window_s);
float gait_double_support_ratio(uint16_t *heel, uint16_t *toe, int n,
                                 uint16_t threshold);
float gait_festination_index(float *stride_history, int n);

#endif