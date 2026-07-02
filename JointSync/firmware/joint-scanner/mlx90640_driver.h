/**
 * JointSync Joint Scanner — MLX90640 Driver Interface
 *
 * License: MIT
 */

#ifndef MLX90640_DRIVER_H
#define MLX90640_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

/**
 * Initialize MLX90640 thermal array.
 */
esp_err_t mlx90640_init(void);

/**
 * Read a full 32×24 thermal frame.
 * pixels: array of 768 int16_t (centi-degrees C).
 */
esp_err_t mlx90640_read_frame(int16_t *pixels);

/**
 * Get a single pixel temperature in degrees C.
 */
float mlx90640_get_pixel(int16_t *pixels, int x, int y);

#endif /* MLX90640_DRIVER_H */