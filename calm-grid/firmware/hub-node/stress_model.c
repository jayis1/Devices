/*
 * stress_model.c — TFLite Micro stress model embedding stub
 *
 * In production, this file contains the int8-quantized TFLite model
 * converted to a C array via `xxd -i`. The model is trained in
 * software/ml-pipeline/train_stress_model.py and exported as
 * stress_int8.tflite, then embedded here.
 *
 * The stub provides a minimal valid TFLite header so the build succeeds.
 * At runtime, hub_main.c falls back to a heuristic stress computation
 * if the model fails to load.
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Model architecture (train_stress_model.py):
 *   - Input: (1, 288, 26) — 24h at 5-min resolution, 26 features
 *   - 3 Conv1D branches (vitals, EDA, activity) → concat
 *   - LSTM(128) × 2 layers
 *   - Dense(64) → ReLU → Dropout(0.3)
 *   - Dense(2) → Sigmoid (stress_score, burnout_risk)
 *   - Output: 2 floats in [0,1]
 *   - Quantization: int8 dynamic-range
 *   - Size: ~78KB
 */

const unsigned char stress_model_data[] = {
    0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33, /* TFL3 header */
    0x14, 0x00, 0x20, 0x00, 0x04, 0x00, 0x08, 0x00,
    0x0c, 0x00, 0x10, 0x00, 0x14, 0x00, 0x00, 0x00,
    0x18, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x02, 0x00,
    /* ... full model data would follow ... */
    0x00, 0x00, 0x00, 0x00,  /* EOF marker */
};

const unsigned int stress_model_len = sizeof(stress_model_data);