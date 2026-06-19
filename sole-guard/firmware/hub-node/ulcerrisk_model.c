/*
 * ulcerrisk_model.c — SoleGuard ulcer-risk TFLite Micro model wrapper
 *
 * Embeds the quantized CNN-LSTM ulcer-risk model (converted via
 * xxd -i). Input: 30 floats (12 pressure zones + 8 temp-asymmetry
 * + 8 gait + 1 edema + 1 ankle temp). Output: 2 floats (risk_L, risk_R).
 *
 * The model itself is trained in software/ml-pipeline/train_ulcer_risk.py
 * and exported as a .tflite (int8 quantized, <80KB).
 *
 * SPDX-License-Identifier: MIT
 */
#include "tflite_micro.h"

/* Placeholder: in production, include the generated model array.
 *   xxd -i ulcer_risk_int8.tflite > ulcer_risk_model_data.h
 * The array below is a stub — the real model replaces it. */
__attribute__((weak)) const unsigned char ulcerrisk_model_data[] = {
    0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33, /* TFL3 magic */
    0x14, 0x00, 0x20, 0x00, 0x1c, 0x00, 0x18, 0x00,
    0x14, 0x00, 0x10, 0x00, 0x0c, 0x00, 0x00, 0x00,
    /* ... full model bytes ... */
    0x00, 0x00, 0x00, 0x00,
};
__attribute__((weak)) const size_t ulcerrisk_model_len = sizeof(ulcerrisk_model_data);

/* TFLite Micro arena — 64KB working area (model needs ~48KB) */
static uint8_t tensor_arena[64 * 1024];

TfLiteMicroModel *tflm_model_create(const unsigned char *data, size_t len)
{
    return tflite_micro_load(data, len, tensor_arena, sizeof(tensor_arena));
}