/**
 * MigraineSync — Hub Edge ML Header
 * ==================================
 * License: MIT
 */

#ifndef HUB_EDGE_ML_H
#define HUB_EDGE_ML_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Edge ML model types.
 */
typedef enum {
    EDGE_MODEL_PRODROME_DETECTOR = 0,
    EDGE_MODEL_ONSET_PREDICTOR   = 1,
} edge_model_t;

/**
 * Edge ML input features (gathered from sensor cache).
 */
typedef struct {
    float hrv_rmssd;           /* current HRV (ms) */
    float hrv_baseline;        /* user's 30-day HRV baseline (ms) */
    float skin_temp;           /* current skin temp (°C) */
    float skin_temp_slope;     /* 6-hour skin temp slope (°C/h) */
    float pressure_hpa;        /* current barometric pressure */
    float pressure_delta_3h;   /* 3-hour pressure change (hPa) */
    float light_lux;           /* current ambient light */
    float hydration_ml;        /* cumulative daily intake (ml) */
    float sleep_score;         /* last night's sleep quality (0-100) */
    float activity_level;      /* current activity (0-4) */
    float stress_index;       /* derived from HRV (0-100) */
} edge_features_t;

/**
 * Edge ML output — risk assessment.
 */
typedef struct {
    float risk_score;          /* 0-100 */
    float prodrome_prob;       /* 0-1 */
    float onset_prob_48h;      /* 0-1 */
    float confidence;          /* 0-1 */
} edge_result_t;

/**
 * Initialize edge ML (load tflite-micro models from flash).
 */
int edge_ml_init(void);

/**
 * Run edge inference with gathered features.
 */
int edge_ml_infer(const edge_features_t *features, edge_result_t *result);

#endif /* HUB_EDGE_ML_H */