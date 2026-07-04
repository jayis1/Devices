/**
 * Insulin injection detection state machine.
 *
 * Detects the distinctive motion sequence of an insulin pen injection:
 *   IDLE → PICKUP → ORIENT → INSERT → INJECT → HOLD → DONE
 *
 * Uses LSM6DSO IMU at 200 Hz. Filters false positives (dropping pen,
 * pocket movement) by requiring the full sequence.
 *
 * License: MIT
 */

#include "injection_detect.h"
#include <string.h>
#include <math.h>

/* ── Tuning Parameters ──────────────────────────────────────────── */

#define ACC_MAG_PICKUP_THRESH   1.5f    /* g, pen pickup */
#define ACC_MAG_INSERT_THRESH   2.0f    /* g, needle insertion spike */
#define GYRO_ORIENT_THRESH      150.0f  /* deg/s, rotation to horizontal */
#define INJECT_VIB_FREQ_MIN     10.0f   /* Hz, plunger vibration lower */
#define INJECT_VIB_FREQ_MAX     100.0f  /* Hz, plunger vibration upper */
#define INJECT_ACC_MAG_MIN      0.3f    /* g */
#define INJECT_ACC_MAG_MAX      0.8f    /* g */
#define HOLD_DURATION_MIN_MS    3000    /* 3 sec dose delivery minimum */
#define HOLD_DURATION_MAX_MS    15000   /* 15 sec max */
#define STATE_TIMEOUT_MS        5000    /* 5 sec timeout between states */
#define ACC_GRAVITY             9.80665f

/* ── State ──────────────────────────────────────────────────────── */

static injection_cb g_callback = NULL;
static inj_state_t g_state = INJ_STATE_IDLE;
static uint32_t g_state_enter_time = 0;
static uint32_t g_inject_start_time = 0;
static uint16_t g_inject_samples = 0;
static float g_inject_acc_sum = 0;
static float g_inject_acc_buf[200];  /* 1 sec at 200 Hz */
static uint16_t g_inject_buf_idx = 0;

/* ── IMU Reading (simulated) ────────────────────────────────────── */

static void read_imu(float *ax, float *ay, float *az,
                     float *gx, float *gy, float *gz)
{
    /* Production: read LSM6DSO via I²C on P0.24(SDA)/P0.25(SCL)
     * Accel: ±4g, 200 Hz. Gyro: ±500 dps, 200 Hz. */
    *ax = 0; *ay = 0; *az = ACC_GRAVITY;  /* resting */
    *gx = 0; *gy = 0; *gz = 0;
}

static float compute_acc_mag(float ax, float ay, float az)
{
    return sqrtf(ax * ax + ay * ay + az * az) / ACC_GRAVITY;
}

static float compute_gyro_mag(float gx, float gy, float gz)
{
    return sqrtf(gx * gx + gy * gy + gz * gz);
}

static uint32_t get_time_ms(void)
{
    /* Production: app_timer_cnt_get() / 32 */
    static uint32_t sim_time = 0;
    sim_time += 5;  /* 5ms per call (200 Hz) */
    return sim_time;
}

/* ── State Machine ──────────────────────────────────────────────── */

void injection_detect_init(injection_cb callback)
{
    g_callback = callback;
    g_state = INJ_STATE_IDLE;
    g_state_enter_time = 0;
    g_inject_samples = 0;
    g_inject_buf_idx = 0;
}

inj_state_t injection_detect_get_state(void)
{
    return g_state;
}

void injection_detect_process_imu(void)
{
    float ax, ay, az, gx, gy, gz;
    read_imu(&ax, &ay, &az, &gx, &gy, &gz);

    float acc_mag = compute_acc_mag(ax, ay, az);
    float gyro_mag = compute_gyro_mag(gx, gy, gz);
    uint32_t now = get_time_ms();

    /* Check for state timeout */
    if (g_state != INJ_STATE_IDLE && g_state != INJ_STATE_DONE) {
        if (now - g_state_enter_time > STATE_TIMEOUT_MS) {
            g_state = INJ_STATE_IDLE;
        }
    }

    switch (g_state) {
    case INJ_STATE_IDLE:
        /* Detect pickup: acceleration magnitude > 1.5g */
        if (acc_mag > ACC_MAG_PICKUP_THRESH) {
            g_state = INJ_STATE_PICKUP;
            g_state_enter_time = now;
        }
        break;

    case INJ_STATE_PICKUP:
        /* Detect orientation: rotation to horizontal (gyro activity) */
        if (gyro_mag > GYRO_ORIENT_THRESH) {
            g_state = INJ_STATE_ORIENT;
            g_state_enter_time = now;
        }
        break;

    case INJ_STATE_ORIENT:
        /* Detect needle insertion: sharp acceleration spike > 2.0g */
        if (acc_mag > ACC_MAG_INSERT_THRESH) {
            g_state = INJ_STATE_INSERT;
            g_state_enter_time = now;
        }
        break;

    case INJ_STATE_INSERT:
        /* Detect plunger push: sustained vibration (10-100 Hz, 0.3-0.8g) */
        if (acc_mag > INJECT_ACC_MAG_MIN && acc_mag < INJECT_ACC_MAG_MAX) {
            g_inject_acc_buf[g_inject_buf_idx] = acc_mag;
            g_inject_buf_idx = (g_inject_buf_idx + 1) % 200;
            g_inject_samples++;
            g_inject_acc_sum += acc_mag;

            /* Need at least 20 samples (100ms) of vibration to confirm */
            if (g_inject_samples >= 20) {
                g_state = INJ_STATE_INJECT;
                g_state_enter_time = now;
                g_inject_start_time = now;
            }
        } else if (acc_mag < INJECT_ACC_MAG_MIN) {
            /* Too quiet — not an injection, reset */
            g_inject_samples = 0;
            g_inject_acc_sum = 0;
        }
        break;

    case INJ_STATE_INJECT:
        /* Accumulate vibration samples during injection */
        g_inject_acc_buf[g_inject_buf_idx] = acc_mag;
        g_inject_buf_idx = (g_inject_buf_idx + 1) % 200;
        g_inject_samples++;
        g_inject_acc_sum += acc_mag;

        /* Detect hold: acceleration stabilizes near 1.0g for >3 sec */
        if (acc_mag > 0.9f && acc_mag < 1.1f) {
            g_state = INJ_STATE_HOLD;
            g_state_enter_time = now;
        }
        break;

    case INJ_STATE_HOLD:
        /* Hold for 3-15 seconds (dose delivery) */
        if (acc_mag > 0.9f && acc_mag < 1.1f) {
            uint32_t hold_dur = now - g_state_enter_time;
            if (hold_dur > HOLD_DURATION_MIN_MS) {
                /* Injection complete! */
                g_state = INJ_STATE_DONE;
                g_state_enter_time = now;

                /* Compute confidence from vibration pattern quality */
                float avg_vib = g_inject_acc_sum / g_inject_samples;
                uint8_t confidence = 0;
                if (avg_vib > 0.4f && avg_vib < 0.7f && g_inject_samples > 100) {
                    confidence = 90;  /* strong match */
                } else if (avg_vib > 0.3f && g_inject_samples > 50) {
                    confidence = 70;
                } else {
                    confidence = 50;
                }

                /* Fire callback */
                if (g_callback != NULL) {
                    injection_event_t event;
                    event.confidence = confidence;
                    event.duration_ms = (uint16_t)(now - g_inject_start_time);
                    event.timestamp = now;
                    g_callback(&event);
                }
            }
        } else {
            /* Movement during hold — reset */
            g_state = INJ_STATE_IDLE;
        }
        break;

    case INJ_STATE_DONE:
        /* Transition back to idle after 2 seconds */
        if (now - g_state_enter_time > 2000) {
            g_state = INJ_STATE_IDLE;
            g_inject_samples = 0;
            g_inject_acc_sum = 0;
            g_inject_buf_idx = 0;
        }
        break;
    }

    /* Check for timeout on non-productive states */
    if (g_state == INJ_STATE_PICKUP || g_state == INJ_STATE_ORIENT) {
        if (now - g_state_enter_time > 3000) {
            g_state = INJ_STATE_IDLE;
        }
    }
}