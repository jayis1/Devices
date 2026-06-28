/*
 * Pest Sentinel — On-device YOLOv8-nano Pest Classifier
 * firmware/pest-sentinel/pest_cnn.c
 *
 * Runs int8-quantized YOLOv8-nano on ESP32-S3 with PSRAM.
 * In production, uses TensorFlow Lite Micro or ESP-DL.
 * This is the inference + post-processing reference implementation.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <math.h>

#include "psp_protocol.h"

static const char *TAG = "PEST_CNN";

extern camera_fb_t *camera_get_frame(void);
extern bool camera_frame_available(void);

extern uint8_t  g_latest_pest_class;
extern uint8_t  g_latest_confidence;
extern uint16_t g_detection_count;
extern float    g_thermal_max_c;
extern bool     g_ir_illumination_on;

/* Model input dimensions (YOLOv8-nano quantized) */
#define MODEL_INPUT_W   320
#define MODEL_INPUT_H    320
#define MODEL_INPUT_C    3   /* RGB */
#define NUM_CLASSES      15

/* Detection result */
typedef struct {
    uint8_t class_id;
    float confidence;
    float x, y, w, h;  /* bounding box (normalized 0-1) */
} detection_t;

/* Placeholder: in production, load .tflite model from PSRAM partition
 * and run via TFLite Micro interpreter.
 * Here we simulate inference with a lightweight heuristic that combines
 * thermal data + image statistics for a reasonable detection estimate. */

static void preprocess_frame(camera_fb_t *fb, int8_t *model_input)
{
    /* Downsample 320×240 RGB565 → 320×320 int8 RGB
     * In production: bilinear resize, normalize, quantize */
    if (!fb || !model_input) return;

    /* Simplified: copy and pad (real impl uses image resize lib) */
    int src_w = fb->width;   /* 320 */
    int src_h = fb->height;  /* 240 */
    uint16_t *src = (uint16_t *)fb->buf;

    for (int y = 0; y < MODEL_INPUT_H; y++) {
        for (int x = 0; x < MODEL_INPUT_W; x++) {
            int src_y = (y * src_h) / MODEL_INPUT_H;
            int src_x = (x * src_w) / MODEL_INPUT_W;
            if (src_y >= src_h) src_y = src_h - 1;
            if (src_x >= src_w) src_x = src_w - 1;

            uint16_t pixel = src[src_y * src_w + src_x];
            /* RGB565 → RGB888 */
            uint8_t r = (pixel >> 11) & 0x1F;
            uint8_t g = (pixel >> 5) & 0x3F;
            uint8_t b = pixel & 0x1F;

            /* Quantize to int8 (-128 to 127) */
            int idx = (y * MODEL_INPUT_W + x) * MODEL_INPUT_C;
            model_input[idx + 0] = (int8_t)((r / 31.0f) * 255 - 128);
            model_input[idx + 1] = (int8_t)((g / 63.0f) * 255 - 128);
            model_input[idx + 2] = (int8_t)((b / 31.0f) * 255 - 128);
        }
    }
}

static int run_inference(const int8_t *input, detection_t *detections, int max_det)
{
    /* In production: TFLite Micro interpreter runs YOLOv8-nano.
     * Here: heuristic simulation using thermal + image features.
     *
     * If thermal_max > 30°C (warm body), classify as rodent (mouse or rat).
     * If image has dark small regions in crepuscular hours, classify as cockroach.
     * This is a PLACEHOLDER — real system uses the actual CNN. */

    int num_det = 0;

    /* Thermal-based rodent detection */
    if (g_thermal_max_c > 30.0f) {
        detections[num_det].class_id = (g_thermal_max_c > 35.0f && rand() % 2) ?
                                       PEST_NORWAY_RAT : PEST_HOUSE_MOUSE;
        detections[num_det].confidence = 0.65f + (g_thermal_max_c - 30.0f) / 40.0f;
        if (detections[num_det].confidence > 0.95f) detections[num_det].confidence = 0.95f;
        detections[num_det].x = 0.3f;
        detections[num_det].y = 0.4f;
        detections[num_det].w = 0.2f;
        detections[num_det].h = 0.15f;
        num_det++;
    }

    /* Image-based insect detection (simulated) */
    /* In production: CNN output grid → NMS → detections */
    if (num_det == 0 && (rand() % 10) > 7) {
        /* Simulated low-confidence insect detection */
        detections[num_det].class_id = PEST_GERMAN_ROACH;
        detections[num_det].confidence = 0.45f;
        detections[num_det].x = 0.2f;
        detections[num_det].y = 0.5f;
        detections[num_det].w = 0.05f;
        detections[num_det].h = 0.03f;
        num_det++;
    }

    return num_det;
}

static void non_max_suppression(detection_t *dets, int *num_det, float iou_thresh)
{
    /* Simple NMS: remove overlapping detections of same class */
    for (int i = 0; i < *num_det; i++) {
        for (int j = i + 1; j < *num_det; j++) {
            if (dets[i].class_id != dets[j].class_id) continue;
            /* Compute IoU (simplified) */
            float overlap = 0.3f; /* placeholder */
            if (overlap > iou_thresh) {
                if (dets[i].confidence < dets[j].confidence) {
                    dets[i] = dets[j];
                }
                /* Shift last over j */
                dets[j] = dets[*num_det - 1];
                (*num_det)--;
                j--;
            }
        }
    }
}

void pest_cnn_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Pest CNN task started (YOLOv8-nano int8)");

    /* Allocate model input buffer in PSRAM */
    int8_t *model_input = (int8_t *)heap_caps_calloc(
        MODEL_INPUT_W * MODEL_INPUT_H * MODEL_INPUT_C,
        sizeof(int8_t), MALLOC_CAP_SPIRAM);

    if (!model_input) {
        ESP_LOGE(TAG, "Failed to allocate model input in PSRAM");
        vTaskDelete(NULL);
        return;
    }

    detection_t detections[20];

    while (1) {
        /* Wait for new frame from camera task */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!camera_frame_available()) continue;

        camera_fb_t *fb = camera_get_frame();
        if (!fb) continue;

        ESP_LOGD(TAG, "Frame: %dx%d format=%d len=%d",
                 fb->width, fb->height, fb->format, fb->len);

        /* Preprocess */
        preprocess_frame(fb, model_input);

        /* Inference */
        int num_det = run_inference(model_input, detections, 20);

        /* NMS */
        non_max_suppression(detections, &num_det, 0.5f);

        /* Select best detection */
        if (num_det > 0 && detections[0].confidence > 0.4f) {
            g_latest_pest_class = detections[0].class_id;
            g_latest_confidence = (uint8_t)(detections[0].confidence * 100);
            g_detection_count++;

            ESP_LOGI(TAG, "🎯 Pest detected: %s (%d%% confidence)",
                     pest_class_name(g_latest_pest_class),
                     g_latest_confidence);

            /* Critical alert for termites */
            if (g_latest_pest_class == PEST_TERMITE_SWARMER) {
                ESP_LOGW(TAG, "🚨 CRITICAL: Termite swarmer detected!");
            }
        } else {
            g_latest_pest_class = PEST_NONE;
            g_latest_confidence = 0;
        }
    }
}