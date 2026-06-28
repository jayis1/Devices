/*
 * Hub — Edge ML (Trap Placement Optimizer + Activity Pattern)
 * firmware/hub/edge_ml.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

#include "psp_protocol.h"

static const char *TAG = "EDGE_ML";

extern uint16_t activity_heatmap[24];

/* Simple K-means clustering on activity heatmap to find optimal trap placement zones.
 * In production, this would use TFLite Micro with a trained model.
 * Here we use a lightweight heuristic: find the hours with highest activity. */

static void optimize_trap_placement(void)
{
    /* Find peak activity hours */
    uint16_t max_activity = 0;
    uint8_t peak_hour = 0;
    uint16_t total_activity = 0;

    for (int h = 0; h < 24; h++) {
        total_activity += activity_heatmap[h];
        if (activity_heatmap[h] > max_activity) {
            max_activity = activity_heatmap[h];
            peak_hour = (uint8_t)h;
        }
    }

    if (total_activity == 0) {
        ESP_LOGI(TAG, "No pest activity detected yet — placement optimization pending");
        return;
    }

    /* Classify activity pattern */
    uint16_t nocturnal = activity_heatmap[0] + activity_heatmap[1] + activity_heatmap[2] +
                         activity_heatmap[3] + activity_heatmap[22] + activity_heatmap[23];
    uint16_t diurnal = activity_heatmap[9] + activity_heatmap[10] + activity_heatmap[11] +
                       activity_heatmap[12] + activity_heatmap[13] + activity_heatmap[14];
    uint16_t crepuscular = activity_heatmap[18] + activity_heatmap[19] + activity_heatmap[20] +
                           activity_heatmap[21];

    const char *pattern;
    if (nocturnal > diurnal && nocturnal > crepuscular)
        pattern = "nocturnal (rodent likely)";
    else if (crepuscular > diurnal)
        pattern = "crepuscular (cockroach likely)";
    else
        pattern = "diurnal (ant/fly likely)";

    ESP_LOGI(TAG, "📊 Activity pattern: %s", pattern);
    ESP_LOGI(TAG, "   Peak hour: %02d:00 (%d detections)", peak_hour, max_activity);
    ESP_LOGI(TAG, "   Total 24h: %d detections", total_activity);

    /* Recommend placement: highest activity zone */
    if (max_activity > 5) {
        ESP_LOGI(TAG, "   → Recommend: deploy traps near sentinel at peak hour %02d:00", peak_hour);
    }

    /* Infestation risk estimation (simplified) */
    float risk = 0;
    if (total_activity > 50) risk = 0.9f;
    else if (total_activity > 20) risk = 0.6f;
    else if (total_activity > 5) risk = 0.3f;
    else risk = 0.1f;

    ESP_LOGI(TAG, "   Estimated 30-day infestation risk: %.0f%%", risk * 100);

    if (risk > 0.5f) {
        ESP_LOGW(TAG, "⚠️  Elevated infestation risk — recommend treatment");
    }
}

void edge_ml_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Edge ML task started (Trap Placement Optimizer)");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(300000)); /* 5-minute interval */
        optimize_trap_placement();
    }
}