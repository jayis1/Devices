/**
 * JointSync Joint Tag — Sensor Fusion Interface
 *
 * License: MIT
 */

#ifndef SENSOR_FUSION_H
#define SENSOR_FUSION_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float w, x, y, z;
} quaternion_t;

void sensor_fusion_init(float sample_rate_hz);
void sensor_fusion_update(int16_t ax_mg, int16_t ay_mg, int16_t az_mg,
                           int16_t gx_mdps, int16_t gy_mdps, int16_t gz_mdps);

/**
 * Get current joint ROM (range of motion) in degrees.
 */
float sensor_fusion_get_rom(void);

/**
 * Get current orientation quaternion.
 */
quaternion_t sensor_fusion_get_quaternion(void);

/**
 * Set current orientation as calibration reference (zero angle).
 */
void sensor_fusion_set_reference(void);

/**
 * Reset ROM tracking for new measurement cycle.
 */
void sensor_fusion_reset_rom(void);

#endif /* SENSOR_FUSION_H */