/*
 * wound_model.c — SoleGuard foot-scan wound-detection TFLite wrapper
 *
 * MobileNetV3-Small, int8 quantized, 7 classes:
 *   0=normal 1=callus 2=blister 3=fissure 4=ulcer 5=fungal 6=maceration
 * Input: 224x224x3 (RGB). Output: 7 logits (softmax applied by caller).
 *
 * Trained in software/ml-pipeline/train_wound_detect.py.
 * Model size: ~2.5MB int8.
 *
 * SPDX-License-Identifier: MIT
 */
#include "tflite_micro.h"

/* Placeholder stub — replace with xxd -i wound_detect_int8.tflite output */
__attribute__((weak)) const unsigned char wound_detect_model_data[] = {
    0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33,
    0x14, 0x00, 0x20, 0x00, 0x1c, 0x00, 0x18, 0x00,
    0x00, 0x00, 0x00, 0x00,
};
__attribute__((weak)) const size_t wound_detect_model_len =
    sizeof(wound_detect_model_data);