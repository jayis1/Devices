/*
 * SeizureSync — SeizureNet edge inference (Hub-side ensemble)
 *
 * Runs tflite-micro SeizureNet (seizure detection) + SUDEPNet (apnea).
 * Cross-validates band-detected events with BCG motion energy.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SEIZURENET_EDGE_H
#define SEIZURENET_EDGE_H

#include <stdint.h>
#include <stdbool.h>

#define BCG_MOTION_THRESHOLD   1200.0f   /* ADC count variance */

void   seizurenet_edge_init(void);
float  seizurenet_infer_seizure(const float *accel, const float *ppg,
                                 const float *eda, int len);
int    seizurenet_infer_sudep(const float *bcg, const float *spo2,
                               int len);
float  bcg_get_motion_energy(void);
void   sudep_alert_trigger(int apnea_s, float spo2, float br);

#endif