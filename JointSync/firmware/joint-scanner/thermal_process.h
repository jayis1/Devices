/**
 * JointSync Joint Scanner — Thermal Processing Interface
 *
 * License: MIT
 */

#ifndef THERMAL_PROCESS_H
#define THERMAL_PROCESS_H

#include <stdint.h>

typedef struct {
    float max_temp;           /* Maximum temperature in frame (°C) */
    float mean_temp;          /* Mean temperature (°C) */
    float thermal_asymmetry;  /* |left - right| mean temperature (°C) */
    uint8_t swelling_grade;   /* 0=normal, 1=mild, 2=moderate, 3=severe */
    uint8_t hotspot_x;        /* Hot spot X coordinate (0-31) */
    uint8_t hotspot_y;        /* Hot spot Y coordinate (0-23) */
} thermal_result_t;

/**
 * Process a 32×24 thermal frame and extract clinical metrics.
 */
thermal_result_t thermal_process_frame(int16_t *pixels);

/**
 * Bilinear interpolation from 32×24 to 64×48 for display.
 */
void thermal_process_interpolate(int16_t *pixels_32x24, float *output_64x48);

#endif /* THERMAL_PROCESS_H */