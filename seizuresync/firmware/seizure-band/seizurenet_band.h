/*
 * SeizureSync — SeizureNet band-side inference
 *
 * 8-layer 1D CNN, ~120K params, quantized int8, tflite-micro on ESP32-S3.
 * Input: 2 s accel (2000 Hz) + PPG (100 Hz) + EDA (4 Hz)
 * Output: 4-class softmax (seizure/syncope/motion/rest)
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SEIZURENET_BAND_H
#define SEIZURENET_BAND_H
#include <stdint.h>
#include <stdbool.h>

void  seizurennet_init(void);
float seizurennet_infer(const float *accel, const float *ppg,
                        const float *eda, int alen, int plen, int elen);
void  trigger_seizure_alert(float confidence);
void  send_heartbeat(void);
void  haptic_pulse(int count);
void  ble_stream_signal(const float *accel, const float *ppg, const float *eda);

#endif