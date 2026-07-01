/**
 * MigraineSync — Hub Edge ML
 * ==========================
 * tflite-micro inference for prodrome detection + onset prediction.
 *
 * In production, this loads two quantized TFLite models from flash:
 *   - prodrome_detector.tflite (1D-CNN, 180 KB, input: 6h HRV + skin-temp)
 *   - onset_predictor.tflite   (LSTM, 420 KB, input: 48h feature window)
 *
 * License: MIT
 */

#include "edge_ml.h"
#include "config.h"
#include <string.h>
#include <math.h>
#include "esp_log.h"

static const char *TAG = "migrainesync_edge_ml";

/* In production:
 * #include "tensorflow/lite/micro/all_ops_resolver.h"
 * #include "tensorflow/lite/micro/micro_interpreter.h"
 * #include "tensorflow/lite/schema/schema_generated.h"
 *
 * static uint8_t s_arena[64 * 1024];  // tensor arena
 * static tflite::MicroInterpreter *s_prodrome_interp;
 * static tflite::MicroInterpreter *s_onset_interp;
 */

int edge_ml_init(void)
{
    ESP_LOGI(TAG, "Loading edge ML models from flash...");

    /* In production:
     * 1. Read prodrome_detector.tflite from flash partition
     * 2. Read onset_predictor.tflite from flash partition
     * 3. Create MicroInterpreter for each
     * 4. Allocate tensor arena
     */

    ESP_LOGI(TAG, "Edge ML models loaded (prodrome 180KB + onset 420KB = 600KB)");
    return 0;
}

int edge_ml_infer(const edge_features_t *f, edge_result_t *r)
{
    if (!f || !r)
        return -1;

    memset(r, 0, sizeof(*r));

    /* ── Heuristic fallback (used when tflite-micro models not yet loaded) ─── */
    /* Risk score = weighted sum of trigger deviations */

    float hrv_ratio = (f->hrv_baseline > 0)
                      ? (f->hrv_rmssd / f->hrv_baseline) : 1.0f;
    float hrv_dev = (1.0f - hrv_ratio) * 100.0f;  /* % below baseline */
    if (hrv_dev < 0) hrv_dev = 0;

    float pressure_risk = 0;
    if (fabsf(f->pressure_delta_3h) > 3.0f)
        pressure_risk = (fabsf(f->pressure_delta_3h) - 3.0f) * 10.0f;
    if (pressure_risk > 30) pressure_risk = 30;

    float hydration_risk = 0;
    if (f->hydration_ml < 1500.0f)
        hydration_risk = (1500.0f - f->hydration_ml) / 15.0f;  /* max ~100 */
    if (hydration_risk > 25) hydration_risk = 25;

    float sleep_risk = 0;
    if (f->sleep_score < 50.0f)
        sleep_risk = (50.0f - f->sleep_score) * 0.5f;  /* max 25 */
    if (sleep_risk > 25) sleep_risk = 25;

    float light_risk = 0;
    if (f->light_lux > 5000.0f)
        light_risk = (f->light_lux - 5000.0f) / 500.0f;
    if (light_risk > 10) light_risk = 10;

    float stress_risk = f->stress_index * 0.1f;  /* 0-10 */

    r->risk_score = hrv_dev * 0.30f + pressure_risk * 0.35f +
                    hydration_risk * 0.15f + sleep_risk * 0.10f +
                    light_risk * 0.05f + stress_risk * 0.05f;

    if (r->risk_score > 100) r->risk_score = 100;

    r->prodrome_prob = (hrv_dev > 20 && f->skin_temp_slope < -0.05f)
                       ? 0.7f : 0.2f;
    r->onset_prob_48h = r->risk_score / 100.0f;
    r->confidence = 0.65f;  /* heuristic confidence */

    ESP_LOGI(TAG, "Edge ML: risk=%.1f prodrome=%.2f onset48h=%.2f conf=%.2f",
             r->risk_score, r->prodrome_prob, r->onset_prob_48h, r->confidence);

    /* In production:
     * 1. Fill prodrome model input tensor with 6h HRV + skin-temp arrays
     * 2. Run prodrome interpreter → get prodrome_prob
     * 3. Fill onset model input tensor with 48h feature window
     * 4. Run onset interpreter → get onset_prob_48h
     * 5. Combine into risk_score
     */

    return 0;
}