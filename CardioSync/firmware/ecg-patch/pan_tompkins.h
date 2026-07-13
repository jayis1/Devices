/*
 * pan_tompkins.h — Pan-Tompkins QRS detection algorithm
 *
 * Real-time R-peak detection for ECG signals, running on nRF52840.
 * Algorithm: bandpass filter → derivative → squaring → moving-window
 * integration → adaptive threshold → R-peak detection.
 *
 * Reference: Pan J, Tompkins WJ. "A Real-Time QRS Detection Algorithm."
 *            IEEE Trans Biomed Eng. 1985;32(3):230-236.
 *
 * License: MIT
 */
#ifndef PAN_TOMPKINS_H
#define PAN_TOMPKINS_H

#include <stdint.h>
#include <stdbool.h>

#define PT_BUFFER_SIZE    512   /* circular buffer for filtering */
#define PT_SAMPLE_RATE    250   /* Hz */
#define PT_ECG_GAIN       1.0f  /* scale factor for raw ADC → μV */

/* Detection state */
typedef struct {
    /* Bandpass filter (5-15 Hz) — biquad cascade */
    float bp_x1, bp_x2;     /* bandpass input history */
    float bp_y1, bp_y2;     /* bandpass output history */
    float lp_y1, lp_y2;     /* lowpass in cascade */

    /* Derivative */
    float deriv_x[4];        /* derivative input history */

    /* Squaring + moving-window integration */
    float mw_buffer[PT_BUFFER_SIZE];  /* moving window buffer */
    int    mw_index;                   /* circular index */
    float  mw_sum;                     /* running sum */

    /* Adaptive threshold */
    float threshold;       /* adaptive detection threshold */
    float signal_avg;      /* running average of signal peaks */
    float noise_avg;       /* running average of noise peaks */
    int   rr_avg1[8];      /* recent RR intervals */
    int   rr_avg2[8];      /* most recent 8 RR intervals */
    int   rr_count;
    int   last_r_idx;      /* sample index of last R-peak */
    int   sample_idx;      /* total sample counter */
    bool  pending_peak;
    float peak_value;
    int   peak_idx;

    /* Results */
    int16_t heart_rate_bpm;
    uint16_t rr_interval_ms;
    bool    r_peak_detected;
} pan_tompkins_t;

void pt_init(pan_tompkins_t *pt);
int  pt_process_sample(pan_tompkins_t *pt, int16_t sample);
     /* Returns 1 if R-peak detected, 0 otherwise */
void pt_update_threshold(pan_tompkins_t *pt, float peak_val, bool is_qrs);

#endif /* PAN_TOMPKINS_H */