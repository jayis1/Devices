/*
 * Hub — Edge ML (TensorFlow Lite Micro)
 * Compost phase classifier — 1D CNN, int8 quantized
 * firmware/hub/edge_ml.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "csp_protocol.h"
#include "sensor_types.h"
#include <math.h>
#include <string.h>

static const char *TAG = "EDGE_ML";

/* Phase classification heuristic (replaces TFLite Micro model for this stub).
 * In production, load a quantized 1D CNN model:
 *   - Input: 8 channels × 24 hours (1 reading/15min = 96 timesteps)
 *   - Channels: temp[3], moisture[3], co2, methane
 *   - Output: 6 phases (mesophilic, thermophilic, cooling, maturation, cured, dormant)
 *
 * The heuristic below uses compost science rules:
 */

#define HISTORY_LEN 96  /* 24 hours at 15-min intervals */
#define NUM_CHANNELS 8

static float history[HISTORY_LEN][NUM_CHANNELS];
static int history_idx = 0;
static int history_count = 0;

/* Circular buffer insert */
static void history_push(const bin_node_data_t *data)
{
    history[history_idx][0] = data->temp_c[0] / 10.0f;      /* temp 1 */
    history[history_idx][1] = data->temp_c[1] / 10.0f;      /* temp 2 */
    history[history_idx][2] = data->temp_c[2] / 10.0f;      /* temp 3 */
    history[history_idx][3] = data->moisture_pct[0] / 100.0f;
    history[history_idx][4] = data->moisture_pct[1] / 100.0f;
    history[history_idx][5] = data->moisture_pct[2] / 100.0f;
    history[history_idx][6] = (float)data->co2_ppm;
    history[history_idx][7] = (float)data->methane_ppm;

    history_idx = (history_idx + 1) % HISTORY_LEN;
    if (history_count < HISTORY_LEN) history_count++;
}

/* Compute average over last N readings for a channel */
static float avg_channel(int channel, int n)
{
    if (history_count == 0) return 0;
    if (n > history_count) n = history_count;
    float sum = 0;
    for (int i = 0; i < n; i++) {
        int idx = (history_idx - 1 - i + HISTORY_LEN) % HISTORY_LEN;
        sum += history[idx][channel];
    }
    return sum / n;
}

/* Compute slope (difference between first and second half averages) */
static float slope_channel(int channel, int n)
{
    if (history_count < n * 2) return 0;
    float first_half = 0, second_half = 0;
    for (int i = 0; i < n; i++) {
        int idx_new = (history_idx - 1 - i + HISTORY_LEN) % HISTORY_LEN;
        int idx_old = (history_idx - 1 - i - n + HISTORY_LEN) % HISTORY_LEN;
        second_half += history[idx_new][channel];
        first_half  += history[idx_old][channel];
    }
    return (second_half - first_half) / n;
}

/* Phase classifier based on compost science */
static uint8_t classify_phase(void)
{
    float temp_avg = avg_channel(1, 12);  /* center temp, last 3 hours */
    float temp_slope = slope_channel(1, 12);
    float co2_avg = avg_channel(6, 12);
    float co2_slope = slope_channel(6, 12);
    float methane_avg = avg_channel(7, 12);
    float moisture_avg = avg_channel(4, 12);

    /* Dormant: very cold, minimal activity */
    if (temp_avg < 5.0f && co2_avg < 200.0f)
        return PHASE_DORMANT;

    /* Thermophilic: high temp (>50°C), high CO2 */
    if (temp_avg > 50.0f && co2_avg > 1000.0f)
        return PHASE_THERMOPHILIC;

    /* Cooling: temp declining from thermophilic, CO2 dropping */
    if (temp_avg > 30.0f && temp_avg < 55.0f && temp_slope < -0.5f && co2_slope < -10.0f)
        return PHASE_COOLING;

    /* Maturation: near ambient, low CO2, stable */
    if (temp_avg < 30.0f && temp_avg > 12.0f && co2_avg < 800.0f && co2_slope < 5.0f)
        return PHASE_MATURATION;

    /* Cured: ambient temp, very low CO2, no methane */
    if (temp_avg < 25.0f && co2_avg < 300.0f && methane_avg < 50.0f)
        return PHASE_CURED;

    /* Default: mesophilic (starting phase) */
    return PHASE_MESOPHILIC;
}

/* C:N ratio estimation heuristic.
 * In production: XGBoost model from sensor fusion features.
 * Heuristic: based on decomposition rate and CO2/methane ratio. */
static float estimate_cn_ratio(void)
{
    float co2 = avg_channel(6, 24);      /* 6h CO2 average */
    float mass_slope = 0; /* would come from weight history */
    float methane = avg_channel(7, 24);
    float temp = avg_channel(1, 24);

    /* High CO2 with low methane = high microbial activity = good C:N */
    /* Low CO2 = either too much carbon (C:N too high) or too little nitrogen */
    /* High methane = anaerobic = C:N imbalance, likely too much nitrogen */

    if (methane > 500.0f) {
        /* Anaerobic: likely too many greens (low C:N) */
        return 15.0f + (1000.0f - fminf(methane, 1000.0f)) / 100.0f * 5.0f;
    }
    if (co2 > 2000.0f && temp > 40.0f) {
        /* Active thermophilic: good C:N around 25-35 */
        return 28.0f;
    }
    if (co2 < 300.0f) {
        /* Low activity: either too much carbon or pile is cured */
        float temp_ambient = 20.0f; /* would come from weather station */
        if (temp < temp_ambient + 3.0f)
            return 20.0f; /* likely cured */
        return 45.0f; /* too much carbon */
    }
    /* Moderate activity */
    return 30.0f + (co2 - 300.0f) / 1700.0f * 10.0f; /* 30-40 range */
}

/* Anaerobic alert detection */
static uint8_t check_alerts(const bin_node_data_t *data)
{
    uint8_t alerts = 0;

    if (data->methane_ppm > 1000) {
        ESP_LOGW(TAG, "⚠️  METHANE HIGH: %d ppm — anaerobic conditions!", data->methane_ppm);
        alerts |= ALERT_METHANE_HIGH;
    }
    if (data->temp_c[1] / 10.0f > 70.0f) {
        ESP_LOGW(TAG, "⚠️  OVERHEAT: %.1f°C — compost too hot!", data->temp_c[1] / 10.0f);
        alerts |= ALERT_OVERHEAT;
    }
    if (data->battery_pct < 20) {
        ESP_LOGW(TAG, "🔋 LOW BATTERY: %d%%", data->battery_pct);
        alerts |= ALERT_LOW_BATTERY;
    }
    if (data->moisture_pct[1] > 70) {
        ESP_LOGW(TAG, "💧 MOISTURE HIGH: %d%% — add dry browns!", data->moisture_pct[1]);
        alerts |= ALERT_MOISTURE_HIGH;
    }
    if (data->moisture_pct[1] > 0 && data->moisture_pct[1] < 30) {
        ESP_LOGW(TAG, "💧 MOISTURE LOW: %d%% — add water!", data->moisture_pct[1]);
        alerts |= ALERT_MOISTURE_LOW;
    }

    return alerts;
}

/* Generate recommendation string */
static void generate_recommendation(uint8_t phase, float cn_ratio,
                                      const bin_node_data_t *data,
                                      char *rec, size_t rec_len)
{
    if (data->alerts & ALERT_METHANE_HIGH) {
        snprintf(rec, rec_len, "🚨 TURN PILE NOW — methane %d ppm indicates anaerobic conditions. Mix in dry browns (cardboard, dry leaves).",
                 data->methane_ppm);
    } else if (data->alerts & ALERT_MOISTURE_HIGH) {
        snprintf(rec, rec_len, "Add dry carbon: shredded cardboard, sawdust, or dry leaves to absorb excess moisture (%d%%).",
                 data->moisture_pct[1]);
    } else if (data->alerts & ALERT_MOISTURE_LOW) {
        snprintf(rec, rec_len, "Add water — moisture is only %d%%. Use a watering can to moisten (not soak) the pile.",
                 data->moisture_pct[1]);
    } else if (phase == PHASE_THERMOPHILIC) {
        snprintf(rec, rec_len, "🔥 Thermophilic phase! Temp %.1f°C. The pile is cooking. Turn in 3-5 days to maintain aeration.",
                 data->temp_c[1] / 10.0f);
    } else if (phase == PHASE_COOLING) {
        snprintf(rec, rec_len, "Cooling phase. Turn the pile one more time to finish decomposition. C:N est: %.0f:1", cn_ratio);
    } else if (phase == PHASE_MATURATION) {
        snprintf(rec, rec_len, "Maturation phase. Almost done! Let it rest 2-3 more weeks. C:N est: %.0f:1", cn_ratio);
    } else if (phase == PHASE_CURED) {
        snprintf(rec, rec_len, "🎉 CURED! Your compost is ready to harvest. Dark, crumbly, earthy-smelling. Use in your garden!");
    } else if (phase == PHASE_DORMANT) {
        snprintf(rec, rec_len, "💤 Pile is dormant (cold). Add nitrogen-rich greens (coffee grounds, vegetable scraps) to restart.");
    } else {
        snprintf(rec, rec_len, "Mesophilic phase. Keep adding materials. Target C:N ~30:1. Current est: %.0f:1", cn_ratio);
    }
}

void edge_ml_task(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    bin_node_data_t data;

    while (1) {
        if (xQueueReceive(queue, &data, portMAX_DELAY) == pdTRUE) {
            /* Add to history */
            history_push(&data);

            /* Need at least 8 readings (2 hours) for meaningful analysis */
            if (history_count < 8) {
                ESP_LOGI(TAG, "Collecting data: %d/%d readings", history_count, 8);
                continue;
            }

            /* Classify phase */
            uint8_t phase = classify_phase();
            float cn_ratio = estimate_cn_ratio();

            /* Check alerts */
            data.alerts = check_alerts(&data);

            /* Generate recommendation */
            char rec[256];
            generate_recommendation(phase, cn_ratio, &data, rec, sizeof(rec));

            ESP_LOGI(TAG, "Phase: %s | C:N ~%.0f:1 | Alerts: 0x%02X",
                     csp_phase_name(phase), cn_ratio, data.alerts);
            ESP_LOGI(TAG, "Recommendation: %s", rec);
        }
    }
}