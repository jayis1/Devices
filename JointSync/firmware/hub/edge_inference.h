/**
 * JointSync Hub — Edge Inference Interface
 *
 * License: MIT
 */

#ifndef EDGE_INFERENCE_H
#define EDGE_INFERENCE_H

#include <stdint.h>

void edge_inference_init(void);

/**
 * Compute joint angle from IMU data using Madgwick AHRS filter.
 * Returns angle in degrees.
 */
float edge_compute_joint_angle(int16_t ax, int16_t ay, int16_t az,
                                int16_t gx, int16_t gy, int16_t gz);

/**
 * Detect inflammation probability from bilateral temperature.
 * Returns 0.0 to 1.0.
 */
float edge_detect_inflammation(float skin_temp, uint8_t sensor_id,
                                uint16_t sender_id);

/**
 * Update cached ROM value for a joint.
 */
void edge_update_rom(uint16_t sender_id, float rom_degrees);

/**
 * Update cached HRV value for a joint.
 */
void edge_update_hrv(uint16_t sender_id, float hrv_ms);

#endif /* EDGE_INFERENCE_H */