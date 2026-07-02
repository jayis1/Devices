/**
 * JointSync Hub — Edge ML Inference
 *
 * tflite-micro on ESP32-S3 for:
 *   - Joint angle computation from IMU data (Madgwick AHRS)
 *   - Inflammation detection from bilateral temperature + ROM + HRV
 *
 * License: MIT
 */

#include "edge_inference.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "edge_inference";

/* ── Madgwick AHRS State ─────────────────────────────────────────── */

typedef struct {
    float q0, q1, q2, q3;  /* Quaternion */
    float beta;             /* Filter gain */
    float sample_dt;        /* Sample interval (sec) */
} ahrs_state_t;

static ahrs_state_t g_ahrs[8];  /* One per tag (up to 8 tags) */

/* ── Temperature History for Bilateral Comparison ────────────────── */

typedef struct {
    float skin_temp;
    float contra_temp;
    float rom_degrees;
    float hrv_ms;
    uint32_t timestamp;
} joint_metrics_t;

static joint_metrics_t g_metrics[8];

/* ── AHRS Functions ──────────────────────────────────────────────── */

static void ahrs_init(ahrs_state_t *ahrs, float sample_rate_hz)
{
    ahrs->q0 = 1.0f;
    ahrs->q1 = 0.0f;
    ahrs->q2 = 0.0f;
    ahrs->q3 = 0.0f;
    ahrs->beta = 0.1f;
    ahrs->sample_dt = 1.0f / sample_rate_hz;
}

static void ahrs_update(ahrs_state_t *ahrs,
                         float gx, float gy, float gz,
                         float ax, float ay, float az)
{
    float q0 = ahrs->q0, q1 = ahrs->q1, q2 = ahrs->q2, q3 = ahrs->q3;
    float dt = ahrs->sample_dt;

    /* Normalize accelerometer */
    float norm = sqrtf(ax * ax + ay * ay + az * az);
    if (norm == 0.0f) return;
    norm = 1.0f / norm;
    ax *= norm; ay *= norm; az *= norm;

    /* Gradient descent corrective step */
    float s0 = 2.0f * (q1 * q3 - q0 * q2) - ax;
    float s1 = 2.0f * (q0 * q1 + q2 * q3) - ay;
    float s2 = 2.0f * (0.5f - q1 * q1 - q2 * q2) - az;

    /* Apply feedback step */
    float recipNorm = 1.0f / sqrtf(s0 * s0 + s1 * s1 + s2 * s2);
    s0 *= recipNorm; s1 *= recipNorm; s2 *= recipNorm;

    /* Compute rate of change of quaternion */
    float dq0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz) - ahrs->beta * s0;
    float dq1 = 0.5f * (q0 * gx + q2 * gz - q3 * gy) - ahrs->beta * s1;
    float dq2 = 0.5f * (q0 * gy - q1 * gz + q3 * gx) - ahrs->beta * s2;
    float dq3 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

    /* Integrate */
    q0 += dq0 * dt;
    q1 += dq1 * dt;
    q2 += dq2 * dt;
    q3 += dq3 * dt;

    /* Normalize quaternion */
    norm = sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    norm = 1.0f / norm;
    ahrs->q0 = q0 * norm;
    ahrs->q1 = q1 * norm;
    ahrs->q2 = q2 * norm;
    ahrs->q3 = q3 * norm;
}

static float ahrs_get_pitch(const ahrs_state_t *ahrs)
{
    /* Pitch from quaternion */
    float sinp = 2.0f * (ahrs->q0 * q1_var(ahrs) + ahrs->q2 * ahrs->q3);
    /* Clamp to avoid gimbal lock */
    if (fabsf(sinp) >= 1.0f) {
        return copysignf(M_PI / 2.0f, sinp);
    }
    return asinf(sinp);
}

static float q1_var(const ahrs_state_t *ahrs)
{
    return ahrs->q1;
}

/* ── Inflammation Detection ──────────────────────────────────────── */

/**
 * Detect inflammation based on:
 *   - Bilateral skin temperature delta (>2.2°C = clinically significant)
 *   - ROM decline
 *   - HRV decrease
 *
 * Returns probability 0.0 to 1.0.
 */
static float compute_inflammation(float skin_temp, float contra_temp,
                                   float rom_degrees, float hrv_ms)
{
    float temp_delta = skin_temp - contra_temp;
    float temp_score = 0.0f;
    float rom_score = 0.0f;
    float hrv_score = 0.0f;

    /* Temperature delta: >2.2°C is clinically significant */
    if (temp_delta > 2.2f) {
        temp_score = 1.0f;
    } else if (temp_delta > 1.0f) {
        temp_score = (temp_delta - 1.0f) / 1.2f;
    }

    /* ROM decline: <80° for knee is concerning (normal ~135°) */
    if (rom_degrees < 60.0f && rom_degrees > 0.0f) {
        rom_score = (90.0f - rom_degrees) / 90.0f;
        if (rom_score > 1.0f) rom_score = 1.0f;
    }

    /* HRV decrease: <20 ms RMSSD is low */
    if (hrv_ms > 0.0f && hrv_ms < 20.0f) {
        hrv_score = (20.0f - hrv_ms) / 20.0f;
    }

    /* Weighted combination */
    float prob = 0.5f * temp_score + 0.3f * rom_score + 0.2f * hrv_score;
    if (prob > 1.0f) prob = 1.0f;
    if (prob < 0.0f) prob = 0.0f;

    return prob;
}

/* ── Public API ───────────────────────────────────────────────────── */

void edge_inference_init(void)
{
    for (int i = 0; i < 8; i++) {
        ahrs_init(&g_ahrs[i], 100.0f);  /* 100 Hz */
        memset(&g_metrics[i], 0, sizeof(joint_metrics_t));
    }
    ESP_LOGI(TAG, "Edge inference initialized (8 AHRS filters at 100 Hz)");
}

float edge_compute_joint_angle(int16_t ax, int16_t ay, int16_t az,
                                int16_t gx, int16_t gy, int16_t gz)
{
    /* Convert milli-g to g, milli-dps to dps */
    float ax_f = ax / 1000.0f;
    float ay_f = ay / 1000.0f;
    float az_f = az / 1000.0f;
    float gx_f = gx / 1000.0f * M_PI / 180.0f;  /* rad/s */
    float gy_f = gy / 1000.0f * M_PI / 180.0f;
    float gz_f = gz / 1000.0f * M_PI / 180.0f;

    /* Use tag 0 by default (simplified — real impl uses sender_id) */
    ahrs_update(&g_ahrs[0], gx_f, gy_f, gz_f, ax_f, ay_f, az_f);

    float pitch = ahrs_get_pitch(&g_ahrs[0]);
    return pitch * 180.0f / M_PI;  /* degrees */
}

float edge_detect_inflammation(float skin_temp, uint8_t sensor_id,
                                uint16_t sender_id)
{
    int idx = sender_id % 8;

    if (sensor_id == 0) {
        /* Skin temperature from affected joint */
        g_metrics[idx].skin_temp = skin_temp;
    } else if (sensor_id == 1) {
        /* Ambient / contralateral reference */
        g_metrics[idx].contra_temp = skin_temp;
    }

    return compute_inflammation(g_metrics[idx].skin_temp,
                                 g_metrics[idx].contra_temp,
                                 g_metrics[idx].rom_degrees,
                                 g_metrics[idx].hrv_ms);
}

void edge_update_rom(uint16_t sender_id, float rom_degrees)
{
    int idx = sender_id % 8;
    g_metrics[idx].rom_degrees = rom_degrees;
}

void edge_update_hrv(uint16_t sender_id, float hrv_ms)
{
    int idx = sender_id % 8;
    g_metrics[idx].hrv_ms = hrv_ms;
}