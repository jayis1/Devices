#ifndef GLUCOSYNC_GLUCOSE_FORECAST_H
#define GLUCOSYNC_GLUCOSE_FORECAST_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Glucose forecast LSTM — runs on ESP32-S3 via tflite-micro.
 * Predicts glucose 30 and 60 minutes ahead.
 */

typedef struct {
    uint16_t glucose_30min;  /* predicted glucose at t+30 (mg/dL) */
    uint16_t glucose_60min;  /* predicted glucose at t+60 (mg/dL) */
    uint8_t  hypo_risk_30;   /* 0-100, prob glucose <70 in 30 min */
    uint8_t  hyper_risk_60;  /* 0-100, prob glucose >180 in 60 min */
    uint8_t  risk_score;     /* 0-100 (fused metabolic risk) */
    uint8_t  recommendation; /* 0=none,1=monitor,2=snack,3=insulin,4=check,5=help */
} glucose_forecast_result_t;

void glucose_forecast_init(void);

/**
 * Run glucose forecast.
 * glucose: array of historical glucose values (mg/dL)
 * trends: array of glucose rate-of-change (centi mg/dL/min)
 * count: number of samples (max 60)
 * iob: insulin on board (units)
 * cob: carbs on board (grams)
 * hr: current heart rate (bpm, 0=unknown)
 * activity_intensity: 0-100
 * result: output predictions
 */
bool glucose_forecast_predict(const uint16_t *glucose,
                               const int16_t *trends,
                               uint8_t count,
                               float iob, float cob,
                               uint8_t hr, uint8_t activity_intensity,
                               glucose_forecast_result_t *result);

#endif