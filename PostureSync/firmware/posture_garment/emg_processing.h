#ifndef EMG_PROCESSING_H
#define EMG_PROCESSING_H

#include <stdint.h>

void emg_compute_rms(const int32_t *samples, int count, uint16_t *rms_out, float mvc);
uint8_t emg_compute_asymmetry(const uint16_t *rms);
float emg_compute_median_freq(const int32_t *samples, int count);
uint8_t emg_compute_fatigue(const uint16_t *current_rms, const float *initial_mvc);
float emg_compute_cocontraction(const uint16_t *rms);

#endif