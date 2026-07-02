/**
 * JointSync Joint Tag — Sensor Fusion (Madgwick AHRS)
 *
 * 6-DoF (accel + gyro) Madgwick filter at 100 Hz.
 * Computes joint orientation quaternion and ROM (range of motion).
 *
 * License: MIT
 */

#include "sensor_fusion.h"
#include <math.h>

#define SAMPLE_RATE  100.0f
#define SAMPLE_DT    (1.0f / SAMPLE_RATE)
#define BETA         0.1f   /* Madgwick filter gain */

static quaternion_t g_q = {1.0f, 0.0f, 0.0f, 0.0f};
static float g_rom = 0.0f;
static float g_max_flexion = 0.0f;
static float g_max_extension = 0.0f;

/* Calibration reference quaternion */
static quaternion_t g_q_ref = {1.0f, 0.0f, 0.0f, 0.0f};
static bool g_has_reference = false;

void sensor_fusion_init(float sample_rate_hz)
{
    g_q.w = 1.0f;
    g_q.x = 0.0f;
    g_q.y = 0.0f;
    g_q.z = 0.0f;
    g_rom = 0.0f;
    g_max_flexion = 0.0f;
    g_max_extension = 0.0f;
    g_has_reference = false;
}

void sensor_fusion_update(int16_t ax_mg, int16_t ay_mg, int16_t az_mg,
                           int16_t gx_mdps, int16_t gy_mdps, int16_t gz_mdps)
{
    /* Convert units */
    float ax = ax_mg / 1000.0f;  /* g */
    float ay = ay_mg / 1000.0f;
    float az = az_mg / 1000.0f;
    float gx = gx_mdps / 1000.0f * M_PI / 180.0f;  /* rad/s */
    float gy = gy_mdps / 1000.0f * M_PI / 180.0f;
    float gz = gz_mdps / 1000.0f * M_PI / 180.0f;

    /* Madgwick AHRS update */
    float q0 = g_q.w, q1 = g_q.x, q2 = g_q.y, q3 = g_q.z;

    /* Normalize accelerometer */
    float norm = sqrtf(ax * ax + ay * ay + az * az);
    if (norm == 0.0f) return;
    norm = 1.0f / norm;
    ax *= norm; ay *= norm; az *= norm;

    /* Gradient descent step */
    float s0 = 2.0f * (q1 * q3 - q0 * q2) - ax;
    float s1 = 2.0f * (q0 * q1 + q2 * q3) - ay;
    float s2 = 2.0f * (0.5f - q1 * q1 - q2 * q2) - az;
    float recipNorm = 1.0f / sqrtf(s0 * s0 + s1 * s1 + s2 * s2);
    s0 *= recipNorm; s1 *= recipNorm; s2 *= recipNorm;

    /* Rate of change */
    float dq0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz) - BETA * s0;
    float dq1 = 0.5f * (q0 * gx + q2 * gz - q3 * gy) - BETA * s1;
    float dq2 = 0.5f * (q0 * gy - q1 * gz + q3 * gx) - BETA * s2;
    float dq3 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

    /* Integrate */
    q0 += dq0 * SAMPLE_DT;
    q1 += dq1 * SAMPLE_DT;
    q2 += dq2 * SAMPLE_DT;
    q3 += dq3 * SAMPLE_DT;

    /* Normalize */
    norm = sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    norm = 1.0f / norm;
    g_q.w = q0 * norm;
    g_q.x = q1 * norm;
    g_q.y = q2 * norm;
    g_q.z = q3 * norm;

    /* Compute ROM (pitch angle relative to reference) */
    if (!g_has_reference) {
        /* First reading becomes reference */
        g_q_ref = g_q;
        g_has_reference = true;
    }

    /* Relative quaternion: q_rel = q_ref⁻¹ * q */
    quaternion_t q_inv = {g_q_ref.w, -g_q_ref.x, -g_q_ref.y, -g_q_ref.z};
    quaternion_t q_rel;
    q_rel.w = q_inv.w * g_q.w - q_inv.x * g_q.x - q_inv.y * g_q.y - q_inv.z * g_q.z;
    q_rel.x = q_inv.w * g_q.x + q_inv.x * g_q.w + q_inv.y * g_q.z - q_inv.z * g_q.y;
    q_rel.y = q_inv.w * g_q.y - q_inv.x * g_q.z + q_inv.y * g_q.w + q_inv.z * g_q.x;
    q_rel.z = q_inv.w * g_q.z + q_inv.x * g_q.y - q_inv.y * g_q.x + q_inv.z * g_q.w;

    /* Extract pitch (flexion/extension) */
    float sinp = 2.0f * (q_rel.w * q_rel.x + q_rel.y * q_rel.z);
    if (sinp > 1.0f) sinp = 1.0f;
    if (sinp < -1.0f) sinp = -1.0f;
    float pitch = asinf(sinp) * 180.0f / M_PI;

    /* Track max flexion and extension for ROM */
    if (pitch > g_max_flexion) g_max_flexion = pitch;
    if (pitch < g_max_extension) g_max_extension = pitch;

    /* ROM = max flexion - max extension */
    g_rom = g_max_flexion - g_max_extension;
}

float sensor_fusion_get_rom(void)
{
    return g_rom;
}

quaternion_t sensor_fusion_get_quaternion(void)
{
    return g_q;
}

void sensor_fusion_set_reference(void)
{
    /* Set current orientation as calibration reference (zero angle) */
    g_q_ref = g_q;
    g_max_flexion = 0.0f;
    g_max_extension = 0.0f;
    g_rom = 0.0f;
}

void sensor_fusion_reset_rom(void)
{
    /* Reset max tracking for new ROM measurement cycle */
    g_max_flexion = 0.0f;
    g_max_extension = 0.0f;
    g_rom = 0.0f;
}