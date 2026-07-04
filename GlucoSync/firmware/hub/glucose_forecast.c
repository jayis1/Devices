/**
 * Glucose forecast — tflite-micro LSTM + hypo warning ensemble.
 * Production: uses tflite-micro to run INT8-quantized model.
 * This stub provides a rule-based fallback when model is not loaded.
 * License: MIT
 */

#include "glucose_forecast.h"
#include <string.h>
#include <math.h>

static bool g_model_loaded = false;

void glucose_forecast_init(void)
{
    /* Production: tflite::MicroInterpreter setup with model in PSRAM.
     * Model: glucose_forecast_lstm_int8.tflite (~32 KB) */
    g_model_loaded = false;  /* Will be true when tflite loads */
}

/**
 * Rule-based glucose prediction fallback.
 * Uses linear extrapolation of current trend, modified by IOB/COB/activity.
 */
static void rule_based_predict(const uint16_t *glucose,
                                const int16_t *trends,
                                uint8_t count,
                                float iob, float cob,
                                uint8_t hr, uint8_t activity_intensity,
                                glucose_forecast_result_t *result)
{
    memset(result, 0, sizeof(*result));

    if (count < 2) {
        result->glucose_30min = glucose[count - 1];
        result->glucose_60min = glucose[count - 1];
        return;
    }

    /* Current trend (avg of last 5 readings) */
    float trend = 0;
    uint8_t n = count < 5 ? count : 5;
    for (uint8_t i = 0; i < n; i++) {
        trend += trends[count - 1 - i] / 100.0f;  /* centi → mg/dL/min */
    }
    trend /= n;

    /* Baseline linear prediction */
    float current = glucose[count - 1];
    float pred_30 = current + trend * 30.0f;
    float pred_60 = current + trend * 60.0f;

    /* Insulin effect: 1 unit IOB → ~30-50 mg/dL drop over 2-3 hours */
    /* Distribute over ~120 min, so ~0.3 mg/dL/min per unit */
    float insulin_drop_30 = iob * 0.3f * 30.0f / 120.0f * 30.0f;
    float insulin_drop_60 = iob * 0.3f * 60.0f / 120.0f * 60.0f;
    pred_30 -= insulin_drop_30;
    pred_60 -= insulin_drop_60;

    /* Carb effect: COB raises glucose over ~2 hours */
    /* 1g carb → ~3-5 mg/dL rise (depends on GI), spread over 120 min */
    float carb_rise_30 = cob * 4.0f * 30.0f / 120.0f * 30.0f / 30.0f;
    float carb_rise_60 = cob * 4.0f * 60.0f / 120.0f;
    pred_30 += carb_rise_30;
    pred_60 += carb_rise_60;

    /* Activity effect: exercise lowers glucose ~0.5-2 mg/dL/min */
    if (activity_intensity > 0 && hr > 0) {
        float drop_rate = 0.01f * activity_intensity;  /* mg/dL/min */
        pred_30 -= drop_rate * 30.0f;
        pred_60 -= drop_rate * 60.0f;
    }

    /* Clamp to physiological range */
    if (pred_30 < 30) pred_30 = 30;
    if (pred_30 > 400) pred_30 = 400;
    if (pred_60 < 30) pred_60 = 30;
    if (pred_60 > 400) pred_60 = 400;

    result->glucose_30min = (uint16_t)pred_30;
    result->glucose_60min = (uint16_t)pred_60;

    /* Hypo risk: probability glucose <70 in 30 min */
    if (pred_30 < 70) {
        result->hypo_risk_30 = 95;
    } else if (pred_30 < 80) {
        result->hypo_risk_30 = (uint8_t)(90 - (pred_30 - 70) * 8);
    } else if (pred_30 < 90) {
        result->hypo_risk_30 = (uint8_t)(30 - (pred_30 - 80) * 3);
    } else {
        result->hypo_risk_30 = 0;
    }

    /* Hyper risk: probability glucose >180 in 60 min */
    if (pred_60 > 250) {
        result->hyper_risk_60 = 90;
    } else if (pred_60 > 180) {
        result->hyper_risk_60 = (uint8_t)(80 - (250 - pred_60) * 0.8);
    } else if (pred_60 > 160) {
        result->hyper_risk_60 = (uint8_t)(20 - (180 - pred_60) * 1);
    } else {
        result->hyper_risk_60 = 0;
    }

    /* Fused risk score */
    float risk = 0;
    if (result->hypo_risk_30 > 50) {
        risk = result->hypo_risk_30;
    } else if (result->hyper_risk_60 > 50) {
        risk = result->hyper_risk_60 * 0.8f;
    } else {
        /* General risk from glucose being outside range */
        if (current < 70) risk = 80;
        else if (current < 80) risk = 40;
        else if (current > 250) risk = 70;
        else if (current > 180) risk = 30;
        else risk = 10;
    }
    result->risk_score = (uint8_t)risk;

    /* Recommendation */
    if (result->hypo_risk_30 > 70) {
        result->recommendation = 2;  /* snack (15g fast-acting carbs) */
    } else if (result->hyper_risk_60 > 60) {
        result->recommendation = 3;  /* insulin */
    } else if (result->risk_score > 50) {
        result->recommendation = 4;  /* check glucose */
    } else if (result->risk_score > 20) {
        result->recommendation = 1;  /* monitor */
    } else {
        result->recommendation = 0;  /* none */
    }
}

bool glucose_forecast_predict(const uint16_t *glucose,
                               const int16_t *trends,
                               uint8_t count,
                               float iob, float cob,
                               uint8_t hr, uint8_t activity_intensity,
                               glucose_forecast_result_t *result)
{
    if (glucose == NULL || trends == NULL || result == NULL || count == 0) {
        return false;
    }

    if (g_model_loaded) {
        /* Production: run tflite-micro inference
         * 1. Build input tensor: [60 timesteps × 8 features]
         *    - glucose[i], trend[i], time_since_meal, carbs, iob, hr, intensity, time_of_day
         * 2. interpreter->Invoke()
         * 3. Read output tensor: [glucose_30, glucose_60, hypo_prob, hyper_prob]
         * 4. Compute risk score + recommendation
         */
    }

    /* Rule-based fallback */
    rule_based_predict(glucose, trends, count, iob, cob, hr,
                       activity_intensity, result);
    return true;
}