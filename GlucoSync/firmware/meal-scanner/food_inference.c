/**
 * Food inference — tflite-micro MobileNetV3-tiny.
 * Production: loads food_carb_cnn_int8.tflite (~900 KB) from flash.
 * This stub provides a simple placeholder.
 * License: MIT
 */

#include "food_inference.h"
#include <string.h>

static bool g_model_loaded = false;

void food_inference_init(void)
{
    /* Production: tflite::MicroInterpreter setup with model in flash.
     * Model: food_carb_cnn_int8.tflite (~900 KB)
     * Input: 224×224×5 (5 spectral bands stacked)
     * Output: [200 food classes (softmax), 1 carb regression, 1 GI regression]
     */
    g_model_loaded = false;
}

bool food_inference_predict(const camera_frame_t *frames,
                            food_inference_result_t *result)
{
    if (frames == NULL || result == NULL) return false;

    memset(result, 0, sizeof(*result));

    if (g_model_loaded) {
        /* Production pipeline:
         * 1. Resize each band to 224×224 (bilinear)
         * 2. Stack 5 bands → input tensor 224×224×5
         * 3. Normalize: (pixel / 127.5) - 1.0
         * 4. interpreter->Invoke()
         * 5. Read classification head → argmax → food_class_id
         * 6. Read carb regression head → carb_grams
         * 7. Read GI regression head → glycemic_index
         * 8. Lookup portion estimate from food class + spectral moisture
         */
    }

    /* Placeholder: return a default "unknown food" result */
    result->food_class_id = 0;
    result->food_confidence = 0;
    result->carb_grams = 0;
    result->portion_grams = 0;
    result->glycemic_index = 50;

    return true;
}