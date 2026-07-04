#ifndef GLUCOSYNC_FOOD_INFERENCE_H
#define GLUCOSYNC_FOOD_INFERENCE_H

#include "camera_driver.h"

/**
 * Food classification + carbohydrate regression via tflite-micro.
 * MobileNetV3-tiny (DM=0.5) with dual heads.
 */

typedef struct {
    uint16_t food_class_id;    /* 0-199 */
    uint8_t  food_confidence;  /* 0-100 */
    uint16_t carb_grams;       /* estimated carbs */
    uint16_t portion_grams;    /* estimated portion */
    uint8_t  glycemic_index;   /* estimated GI (0-100) */
} food_inference_result_t;

void food_inference_init(void);

/**
 * Run food inference on 5-band image stack.
 * frames: array of 5 camera_frame_t (one per spectral band)
 * result: output predictions
 */
bool food_inference_predict(const camera_frame_t *frames,
                            food_inference_result_t *result);

#endif