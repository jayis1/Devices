/**
 * JointSync Joint Scanner — Thermal Processing
 *
 * Processes MLX90640 32×24 thermal frames to extract:
 *   - Max temperature (hot spot)
 *   - Mean temperature
 *   - Thermal asymmetry (left vs right)
 *   - Swelling grade (0-3)
 *
 * License: MIT
 */

#include "thermal_process.h"
#include <math.h>
#include <string.h>

thermal_result_t thermal_process_frame(int16_t *pixels)
{
    thermal_result_t result = {0};

    /* Convert to float array */
    float temps[768];
    for (int i = 0; i < 768; i++) {
        temps[i] = pixels[i] / 100.0f;  /* centi-degrees → degrees C */
    }

    /* Find max and mean */
    float max_temp = -273.15f;
    float sum = 0.0f;
    for (int i = 0; i < 768; i++) {
        if (temps[i] > max_temp) max_temp = temps[i];
        sum += temps[i];
    }
    float mean_temp = sum / 768.0f;

    result.max_temp = max_temp;
    result.mean_temp = mean_temp;

    /* Compute thermal asymmetry (left half vs right half) */
    float left_sum = 0, right_sum = 0;
    int left_count = 0, right_count = 0;
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 16; x++) {
            left_sum += temps[y * 32 + x];
            left_count++;
        }
        for (int x = 16; x < 32; x++) {
            right_sum += temps[y * 32 + x];
            right_count++;
        }
    }
    float left_mean = left_sum / left_count;
    float right_mean = right_sum / right_count;
    result.thermal_asymmetry = left_mean - right_mean;
    if (result.thermal_asymmetry < 0) {
        result.thermal_asymmetry = -result.thermal_asymmetry;
    }

    /* Determine swelling grade based on max temp vs mean */
    float temp_gradient = max_temp - mean_temp;

    if (max_temp > 36.0f && temp_gradient > 1.5f) {
        result.swelling_grade = 3;  /* Severe — hot, localized inflammation */
    } else if (max_temp > 34.5f && temp_gradient > 1.0f) {
        result.swelling_grade = 2;  /* Moderate */
    } else if (max_temp > 33.0f && temp_gradient > 0.5f) {
        result.swelling_grade = 1;  /* Mild */
    } else {
        result.swelling_grade = 0;  /* Normal */
    }

    /* Find hot spot location */
    int hotspot_idx = 0;
    float hotspot_temp = -273.0f;
    for (int i = 0; i < 768; i++) {
        if (temps[i] > hotspot_temp) {
            hotspot_temp = temps[i];
            hotspot_idx = i;
        }
    }
    result.hotspot_x = hotspot_idx % 32;
    result.hotspot_y = hotspot_idx / 32;

    return result;
}

void thermal_process_interpolate(int16_t *pixels_32x24, float *output_64x48)
{
    /* Bilinear interpolation from 32×24 to 64×48 for display */
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 64; x++) {
            float src_x = (float)x * 32.0f / 64.0f;
            float src_y = (float)y * 24.0f / 48.0f;

            int x0 = (int)src_x;
            int y0 = (int)src_y;
            int x1 = (x0 + 1 < 32) ? x0 + 1 : x0;
            int y1 = (y0 + 1 < 24) ? y0 + 1 : y0;

            float dx = src_x - x0;
            float dy = src_y - y0;

            float t00 = pixels_32x24[y0 * 32 + x0] / 100.0f;
            float t01 = pixels_32x24[y0 * 32 + x1] / 100.0f;
            float t10 = pixels_32x24[y1 * 32 + x0] / 100.0f;
            float t11 = pixels_32x24[y1 * 32 + x1] / 100.0f;

            output_64x48[y * 64 + x] = (1.0f - dx) * (1.0f - dy) * t00 +
                                        dx * (1.0f - dy) * t01 +
                                        (1.0f - dx) * dy * t10 +
                                        dx * dy * t11;
        }
    }
}