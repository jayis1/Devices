/**
 * AsthmaSync — Hub Edge ML Header
 *
 * License: MIT
 */

#ifndef EDGE_ML_H
#define EDGE_ML_H

#include "../common/protocol.h"
#include <stdint.h>
#include <stddef.h>

/** Initialize tflite-micro models (wheeze CNN + actuation RF). */
int edge_ml_init(void);

/** Classify respiratory sound from mel-spectrogram input.
 *  @param mel_spectrogram  40×32 uint8 mel bins
 *  @param mel_len         length of input buffer
 *  @param out_probs       22-element probability array
 *  @param out_class       best class index
 *  @return 0 on success */
int edge_ml_classify_wheeze(const uint8_t *mel_spectrogram, size_t mel_len,
                            float *out_probs, int *out_class);

/** Classify inhaler actuation from accelerometer features.
 *  @param accel_features  statistical features (mean, std, peak, etc.)
 *  @param feat_len        number of features
 *  @param out_class       best class index (0-3)
 *  @param out_confidence  confidence 0-1
 *  @return 0 on success */
int edge_ml_classify_actuation(const float *accel_features, size_t feat_len,
                               int *out_class, float *out_confidence);

/** Get human-readable class label. */
const char* edge_ml_wheeze_label(int class_id);
const char* edge_ml_actuation_label(int class_id);

/** Calculate GINA alert zone from current vitals + air quality + rescue use. */
alert_zone_t edge_ml_calc_zone(const vitals_t *vitals,
                               const air_quality_t *air,
                               uint8_t rescue_use_week);

#endif /* EDGE_ML_H */