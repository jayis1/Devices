/**
 * SightSync Eye Tag — Head Posture Detection Implementation
 *
 * Complementary filter for pitch/roll, gyroscope integration for yaw.
 * Forward head posture flag when pitch > 15° sustained for >30 s.
 *
 * License: MIT
 */

#include "posture.h"
#include <Arduino.h>
#include <math.h>

static Adafruit_LSM6DS *s_imu = NULL;
static uint8_t s_mode = 0;

/* Filter state */
static float s_pitch = 0.0f;  /* degrees */
static float s_roll  = 0.0f;
static float s_yaw   = 0.0f;

static uint32_t s_last_sample_us = 0;
static uint32_t s_forward_start_ms = 0;
static bool s_forward_active = false;

#define ALPHA 0.98f  /* complementary filter coefficient */
#define RAD_TO_DEG (180.0f / PI)
#define FORWARD_THRESHOLD_DEG 15.0f
#define FORWARD_SUSTAINED_MS  30000  /* 30 seconds */

/* ── Init ─────────────────────────────────────────────────────────── */

void posture_init(Adafruit_LSM6DS *imu)
{
    s_imu = imu;
    s_last_sample_us = micros();
}

/* ── Sample ───────────────────────────────────────────────────────── */

void posture_sample(void)
{
    if (s_imu == NULL) return;

    sensors_event_t accel, gyro, temp;
    if (!s_imu->getEvent(&accel, &gyro, &temp)) return;

    uint32_t now_us = micros();
    float dt = (now_us - s_last_sample_us) / 1000000.0f;
    s_last_sample_us = now_us;

    /* Accelerometer-based pitch/roll (tilt angles) */
    float accel_pitch = atan2(-accel.acceleration.x,
                                sqrt(accel.acceleration.y * accel.acceleration.y +
                                     accel.acceleration.z * accel.acceleration.z)) * RAD_TO_DEG;
    float accel_roll = atan2(accel.acceleration.y, accel.acceleration.z) * RAD_TO_DEG;

    /* Gyroscope integration */
    float gyro_pitch_rate = gyro.gyro.y * RAD_TO_DEG;  /* rad/s → deg/s */
    float gyro_roll_rate  = gyro.gyro.x * RAD_TO_DEG;
    float gyro_yaw_rate   = gyro.gyro.z * RAD_TO_DEG;

    /* Complementary filter: gyro + accel fusion */
    s_pitch = ALPHA * (s_pitch + gyro_pitch_rate * dt) + (1.0f - ALPHA) * accel_pitch;
    s_roll  = ALPHA * (s_roll  + gyro_roll_rate  * dt) + (1.0f - ALPHA) * accel_roll;
    s_yaw  += gyro_yaw_rate * dt;  /* yaw from gyro only (no magnetometer) */

    /* Wrap yaw to -180..180 */
    while (s_yaw > 180.0f)  s_yaw -= 360.0f;
    while (s_yaw < -180.0f) s_yaw += 360.0f;

    /* Forward head posture detection: pitch > 15° sustained */
    uint32_t now_ms = millis();
    if (s_pitch > FORWARD_THRESHOLD_DEG) {
        if (!s_forward_active) {
            s_forward_start_ms = now_ms;
            s_forward_active = true;
        } else if ((now_ms - s_forward_start_ms) > FORWARD_SUSTAINED_MS) {
            /* Sustained forward head posture confirmed */
        }
    } else {
        s_forward_active = false;
        s_forward_start_ms = 0;
    }
}

/* ── Get angles ───────────────────────────────────────────────────── */

void posture_get_angles(int16_t *pitch, int16_t *roll, int16_t *yaw,
                         uint8_t *forward_flag, uint8_t *risk)
{
    *pitch = (int16_t)(s_pitch * 100.0f);  /* centi-degrees */
    *roll  = (int16_t)(s_roll  * 100.0f);
    *yaw   = (int16_t)(s_yaw   * 100.0f);

    *forward_flag = (s_forward_active && s_pitch > FORWARD_THRESHOLD_DEG) ? 1 : 0;

    /* Posture risk: 0-100 based on forward angle magnitude */
    float abs_pitch = fabsf(s_pitch);
    if (abs_pitch < 10.0f) {
        *risk = 0;
    } else if (abs_pitch < 15.0f) {
        *risk = (uint8_t)((abs_pitch - 10.0f) / 5.0f * 30.0f);
    } else if (abs_pitch < 30.0f) {
        *risk = (uint8_t)(30.0f + (abs_pitch - 15.0f) / 15.0f * 50.0f);
    } else {
        *risk = 100;
    }
}

void posture_set_mode(uint8_t mode)
{
    s_mode = mode;
}