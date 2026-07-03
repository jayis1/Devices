#ifndef DRIVESYNC_EDGE_INFERENCE_H
#define DRIVESYNC_EDGE_INFERENCE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Edge ML inference for DriveSync Dash Hub.
 *
 * Runs tflite-micro models on ESP32-S3:
 * 1. Eye-closure CNN (PERCLOS computation)
 * 2. Head-pose regression CNN
 *
 * Models are quantized INT8, stored in flash.
 */

/**
 * Initialize edge ML (load tflite models from flash).
 */
void edge_inference_init(void);

/**
 * Process a camera frame and extract drowsiness features.
 * Called from camera capture task.
 *
 * @param frame_data  Grayscale pixel data (640x480)
 * @param width        Frame width
 * @param height       Frame height
 * @param out_perclos  Updated PERCLOS (fraction, 0-1)
 * @param out_blink_rate Updated blink rate (blinks/min)
 * @param out_head_pitch Updated head pitch (centi-degrees)
 * @param out_head_yaw Updated head yaw (centi-degrees)
 * @param out_head_roll Updated head roll (centi-degrees)
 * @param out_confidence Updated inference confidence (0-100)
 * @return true on successful inference
 */
bool edge_inference_process_frame(const uint8_t *frame_data,
                                   uint16_t width, uint16_t height,
                                   float *out_perclos,
                                   uint16_t *out_blink_rate,
                                   int16_t *out_head_pitch,
                                   int16_t *out_head_yaw,
                                   int16_t *out_head_roll,
                                   uint8_t *out_confidence);

/**
 * Get the current PERCLOS value (fraction of time eyes >80% closed
 * over the rolling 1-minute window).
 */
float edge_inference_get_perclos(void);

/**
 * Get the current blink rate (blinks per minute).
 */
uint16_t edge_inference_get_blink_rate(void);

/**
 * Get head-bob event count for the last minute.
 */
uint8_t edge_inference_get_head_bob_count(void);

#endif /* DRIVESYNC_EDGE_INFERENCE_H */