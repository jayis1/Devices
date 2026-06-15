/*
 * ErgoFlow — Wearable Tag Node Main
 * nRF52833 based wrist-worn IMU + pulse ox sensor
 *
 * Responsibilities:
 *   - 100Hz 6-axis IMU data acquisition
 *   - 25Hz magnetometer acquisition
 *   - 9-DOF sensor fusion (Madgwick filter)
 *   - Activity classification (typing, mouse, phone, idle, stretch, walk)
 *   - Heart rate sampling (every 60s)
 *   - BLE mesh transmit (orientation + activity + HR)
 *   - Haptic alerts (3 patterns)
 *   - Ultra-low-power scheduling
 *   - Tap detection for UI (double-tap dismiss, triple-tap status)
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#include "common/ble_mesh/mesh_config.h"
#include "common/ble_mesh/mesh_handler.h"
#include "common/ble_mesh/protocol.h"
#include "common/sensors/icm42688.h"
#include "common/sensors/mmc5603.h"
#include "common/sensors/max30101.h"

LOG_MODULE_REGISTER(tag_main, CONFIG_ERGO_LOG_LEVEL);

/* ── Configuration ──────────────────────────────────────────────────── */
#define IMU_SAMPLE_HZ         100    /* 100 Hz IMU */
#define MAG_SAMPLE_HZ         25     /* 25 Hz magnetometer */
#define BLE_TX_INTERVAL_MS    500    /* 2 Hz BLE transmit */
#define HR_SAMPLE_INTERVAL_S  60     /* Heart rate every 60s */
#define ACTIVITY_WINDOW_MS    2000   /* 2s activity window */
#define BATTERY_CHECK_MS      30000  /* Battery check every 30s */

/* ── Madgwick filter parameters ─────────────────────────────────────── */
#define MADGWICK_BETA         0.04f  /* Filter gain */
#define MADGWICK_SAMPLE_HZ   100     /* Match IMU rate */

/* ── Activity classifier thresholds ──────────────────────────────────── */
#define TYPING_ACCEL_VAR      0.005f  /* Low variance = typing */
#define MOUSE_ACCEL_VAR       0.02f   /* Medium variance = mousing */
#define IDLE_ACCEL_VAR        0.001f  /* Very low variance = idle */
#define WALK_ACCEL_PEAK       0.3f    /* High peak = walking */
#define STRETCH_ACCEL_RANGE   0.15f   /* Medium range = stretching */

/* ── Haptic patterns ────────────────────────────────────────────────── */
#define HAPTIC_GENTLE_MS      200    /* Gentle break nudge */
#define HAPTIC_DOUBLE_MS       150    /* Double pulse: posture warning */
#define HAPTIC_URGENT_MS      300    /* Urgent: prolonged sitting */

/* ── Sensor fusion state ────────────────────────────────────────────── */
typedef struct {
    float q[4];          /* Quaternion: w, x, y, z */
    float beta;           /* Madgwick gain */
    float sample_period;  /* 1/sample_rate */
} madgwick_state_t;

static madgwick_state_t madgwick = {
    .q = {1.0f, 0.0f, 0.0f, 0.0f},
    .beta = MADGWICK_BETA,
    .sample_period = 1.0f / MADGWICK_SAMPLE_HZ,
};

/* ── Activity window buffer ──────────────────────────────────────────── */
#define ACTIVITY_BUF_SIZE  200  /* 2 seconds at 100Hz */

typedef struct {
    float accel_x[ACTIVITY_BUF_SIZE];
    float accel_y[ACTIVITY_BUF_SIZE];
    float accel_z[ACTIVITY_BUF_SIZE];
    float gyro_x[ACTIVITY_BUF_SIZE];
    float gyro_y[ACTIVITY_BUF_SIZE];
    float gyro_z[ACTIVITY_BUF_SIZE];
    int buf_index;
    bool buf_full;
} activity_buffer_t;

static activity_buffer_t activity_buf;
static icm42688_accel_t imu_accel;
static icm42688_gyro_t imu_gyro;
static mmc5603_mag_t imu_mag;
static uint8_t current_activity = ERGO_ACTIVITY_IDLE;
static uint8_t activity_confidence = 80;
static float heart_rate_bpm = 0;
static float spo2_pct = 0;
static uint8_t battery_pct = 100;

/* ── Madgwick filter update ─────────────────────────────────────────── */
static void madgwick_update(madgwick_state_t *state,
                             float ax, float ay, float az,
                             float gx, float gy, float gz,
                             float mx, float my, float mz)
{
    float q0 = state->q[0], q1 = state->q[1], q2 = state->q[2], q3 = state->q[3];
    float dt = state->sample_period;

    /* Normalize accelerometer */
    float norm_a = sqrtf(ax * ax + ay * ay + az * az);
    if (norm_a < 0.001f) return;
    ax /= norm_a; ay /= norm_a; az /= norm_a;

    /* Normalize magnetometer */
    float norm_m = sqrtf(mx * mx + my * my + mz * mz);
    if (norm_m < 0.001f) {
        /* No magnetometer data — use gyro-only update */
        gx *= 0.5f * dt;
        gy *= 0.5f * dt;
        gz *= 0.5f * dt;

        float dq0 = -q1*gx - q2*gy - q3*gz;
        float dq1 =  q0*gx + q2*gz - q3*gy;
        float dq2 =  q0*gy - q1*gz + q3*gx;
        float dq3 =  q0*gz + q1*gy - q2*gx;

        state->q[0] += dq0;
        state->q[1] += dq1;
        state->q[2] += dq2;
        state->q[3] += dq3;
    } else {
        mx /= norm_m; my /= norm_m; mz /= norm_m;

        /* Reference direction of Earth's magnetic field */
        float hx = mx * (2*q2*q2 + 2*q3*q3 - 1) +
                    my * (2*q1*q2 - 2*q0*q3) +
                    mz * (2*q1*q3 + 2*q0*q2);
        float hy = mx * (2*q1*q2 + 2*q0*q3) +
                    my * (2*q1*q1 + 2*q3*q3 - 1) +
                    mz * (2*q2*q3 - 2*q0*q1);

        float bx = sqrtf(hx * hx + hy * hy);
        float bz = mx * (2*q1*q3 - 2*q0*q2) +
                    my * (2*q2*q3 + 2*q0*q1) +
                    mz * (2*q0*q0 + 2*q3*q3 - 1);

        /* Gradient descent correction */
        float f0 = 2*(q1*q3 - q0*q2) - ax;
        float f1 = 2*(q0*q1 + q2*q3) - ay;
        float f2 = 2*(q0*q0 + q3*q3) - 0.5f - az;
        float f3 = 2*bx*(0.5f - q2*q2 - q3*q3) + 2*bz*(q1*q3 - q0*q2) - mx;
        float f4 = 2*bx*(q1*q2 - q0*q3) + 2*bz*(q0*q1 + q2*q3) - my;
        float f5 = 2*bx*(q0*q2 + q1*q3) + 2*bz*(0.5f - q1*q1 - q2*q2) - mz;

        /* Jacobian * gradient */
        float J0J = 2*q2*f0 - 2*q1*f1;
        float J1J = 2*q1*f0 + 2*q0*f1 - 4*q1*f2;
        float J2J = 2*q3*f0 - 4*q2*f1 + 2*q0*f2;
        float J3J = 2*q0*f0 + 2*q3*f1 - 4*q3*f2;

        float norm_j = sqrtf(J0J*J0J + J1J*J1J + J2J*J2J + J3J*J3J);
        if (norm_j < 0.001f) norm_j = 1.0f;

        /* Apply feedback */
        float beta_dt = state->beta * dt;
        state->q[0] += (1.0f - beta_dt) * (q0 + (-gx*dt*q1 - gy*dt*q2 - gz*dt*q3) * 0.5f) +
                        beta_dt * (-J0J / norm_j);
        state->q[1] += (1.0f - beta_dt) * (q1 + ( gx*dt*q0 + gy*dt*q3 - gz*dt*q2) * 0.5f) +
                        beta_dt * (-J1J / norm_j);
        state->q[2] += (1.0f - beta_dt) * (q2 + (-gx*dt*q3 + gy*dt*q0 + gz*dt*q1) * 0.5f) +
                        beta_dt * (-J2J / norm_j);
        state->q[3] += (1.0f - beta_dt) * (q3 + ( gx*dt*q2 - gy*dt*q1 + gz*dt*q0) * 0.5f) +
                        beta_dt * (-J3J / norm_j);
    }

    /* Normalize quaternion */
    float norm_q = sqrtf(state->q[0]*state->q[0] + state->q[1]*state->q[1] +
                         state->q[2]*state->q[2] + state->q[3]*state->q[3]);
    if (norm_q > 0.001f) {
        state->q[0] /= norm_q;
        state->q[1] /= norm_q;
        state->q[2] /= norm_q;
        state->q[3] /= norm_q;
    }
}

/* ── Activity classification (simple threshold-based) ────────────────── */
static uint8_t classify_activity(void)
{
    if (!activity_buf.buf_full) return ERGO_ACTIVITY_IDLE;

    /* Compute variance of accelerometer magnitude */
    float sum = 0, sum_sq = 0;
    float peaks = 0;
    float range_max = -10.0f, range_min = 10.0f;

    for (int i = 0; i < ACTIVITY_BUF_SIZE; i++) {
        float mag = sqrtf(activity_buf.accel_x[i] * activity_buf.accel_x[i] +
                          activity_buf.accel_y[i] * activity_buf.accel_y[i] +
                          activity_buf.accel_z[i] * activity_buf.accel_z[i]);
        sum += mag;
        sum_sq += mag * mag;
        if (mag > range_max) range_max = mag;
        if (mag < range_min) range_min = mag;

        /* Count peaks (transitions above threshold) */
        if (i > 0) {
            float prev = sqrtf(activity_buf.accel_x[i-1]*activity_buf.accel_x[i-1] +
                                activity_buf.accel_y[i-1]*activity_buf.accel_y[i-1] +
                                activity_buf.accel_z[i-1]*activity_buf.accel_z[i-1]);
            if (fabsf(mag - prev) > 0.1f) peaks++;
        }
    }

    float mean = sum / ACTIVITY_BUF_SIZE;
    float variance = (sum_sq / ACTIVITY_BUF_SIZE) - (mean * mean);
    float range = range_max - range_min;

    /* Classify based on variance and range */
    if (variance < IDLE_ACCEL_VAR && range < 0.02f) {
        activity_confidence = 85;
        return ERGO_ACTIVITY_IDLE;
    } else if (variance < TYPING_ACCEL_VAR && range < 0.1f) {
        activity_confidence = 75;
        return ERGO_ACTIVITY_TYPING;
    } else if (variance < MOUSE_ACCEL_VAR && peaks > 20) {
        activity_confidence = 70;
        return ERGO_ACTIVITY_MOUSE;
    } else if (range > STRETCH_ACCEL_RANGE && variance < 0.1f) {
        activity_confidence = 65;
        return ERGO_ACTIVITY_STRETCH;
    } else if (peaks > 100 && variance > WALK_ACCEL_PEAK) {
        activity_confidence = 80;
        return ERGO_ACTIVITY_WALK;
    } else if (variance < TYPING_ACCEL_VAR) {
        activity_confidence = 60;
        return ERGO_ACTIVITY_PHONE;
    }

    activity_confidence = 50;
    return ERGO_ACTIVITY_IDLE;
}

/* ── Haptic motor ───────────────────────────────────────────────────── */
static void haptic_pulse(uint16_t duration_ms, uint8_t count, uint16_t gap_ms)
{
    const struct device *gpio_dev = device_get_binding("GPIO_0");
    for (uint8_t i = 0; i < count; i++) {
        gpio_pin_set(gpio_dev, 11, 1);  /* HAPTIC_PWM_PIN */
        k_msleep(duration_ms);
        gpio_pin_set(gpio_dev, 11, 0);
        if (i < count - 1) k_msleep(gap_ms);
    }
}

static void haptic_gentle(void) { haptic_pulse(HAPTIC_GENTLE_MS, 1, 0); }
static void haptic_double(void) { haptic_pulse(HAPTIC_DOUBLE_MS, 2, 100); }
static void haptic_urgent(void) { haptic_pulse(HAPTIC_URGENT_MS, 3, 200); }

/* ── BLE mesh transmit ──────────────────────────────────────────────── */
static void transmit_orientation(void)
{
    ergo_imu_orientation_t msg = {
        .activity = current_activity,
        .confidence = activity_confidence,
    };
    memcpy(msg.quat, madgwick.q, sizeof(msg.quat));

    uint8_t buf[20];
    uint16_t len;
    ergo_pack_imu_orientation(&msg, buf, &len);
    mesh_handler_send(ERGO_OP_IMU_ORIENTATION, buf, len, ERGO_ADDR_HUB);
}

static void transmit_heart_rate(void)
{
    ergo_heart_rate_t msg = {
        .hr_bpm = (uint8_t)heart_rate_bpm,
        .spo2_pct = (uint8_t)spo2_pct,
    };
    uint8_t buf[8];
    uint16_t len;
    ergo_pack_heart_rate(&msg, buf, &len);
    mesh_handler_send(ERGO_OP_HEART_RATE, buf, len, ERGO_ADDR_HUB);
}

static void transmit_heartbeat(void)
{
    ergo_node_heartbeat_t hb = {
        .battery_pct = battery_pct,
        .state = ERGO_STATE_RUNNING,
        .uptime_min = (uint16_t)(k_uptime_get() / 60000),
    };
    uint8_t buf[8];
    uint16_t len;
    ergo_pack_node_heartbeat(&hb, buf, &len);
    mesh_handler_send(ERGO_OP_NODE_HEARTBEAT, buf, len, ERGO_ADDR_HUB);
}

/* ── BLE mesh receive callback ──────────────────────────────────────── */
static void tag_mesh_callback(uint16_t opcode, const uint8_t *data,
                               uint16_t len, uint16_t src_addr, void *user_data)
{
    if (opcode == ERGO_OP_BREAK_REMINDER) {
        ergo_break_reminder_t reminder;
        if (ergo_unpack_break_reminder(data, len, &reminder) == 0) {
            switch (reminder.type) {
                case ERGO_BREAK_STRETCH:   haptic_gentle(); break;
                case ERGO_BREAK_WALK:      haptic_urgent(); break;
                case ERGO_BREAK_LOOK_AWAY: haptic_double(); break;
            }
            LOG_INF("Break reminder: type=%d", reminder.type);
        }
    } else if (opcode == ERGO_OP_POSTURE_SCORE) {
        ergo_posture_score_t score;
        if (ergo_unpack_posture_score(data, len, &score) == 0) {
            if (score.risk_level >= 2) {
                haptic_double();  /* High risk: double pulse */
            }
        }
    } else if (opcode == ERGO_OP_CALIBRATION) {
        /* Re-center IMU */
        madgwick.q[0] = 1.0f; madgwick.q[1] = 0.0f;
        madgwick.q[2] = 0.0f; madgwick.q[3] = 0.0f;
        LOG_INF("IMU calibration reset");
    }
}

/* ── Main ───────────────────────────────────────────────────────────── */
int main(void)
{
    LOG_INF("ErgoFlow Wearable Tag starting...");

    /* Initialize sensors */
    icm42688_init();
    mmc5603_init(MMC5603_I2C_ADDR);

    /* Optionally initialize pulse ox (power-gated) */
    /* max30101_init(); */

    /* Initialize mesh */
    mesh_handler_init();
    mesh_handler_register_callback(0xFFFF, tag_mesh_callback, NULL);

    /* Configure GPIO */
    const struct device *gpio_dev = device_get_binding("GPIO_0");
    gpio_pin_configure(gpio_dev, 11, GPIO_OUTPUT_LOW);  /* Haptic motor */
    gpio_pin_configure(gpio_dev, 12, GPIO_OUTPUT_LOW);  /* Status LED */
    gpio_pin_configure(gpio_dev, 15, GPIO_OUTPUT_HIGH);  /* PULSE_EN (off) */

    LOG_INF("Wearable Tag running");

    uint32_t imu_count = 0;
    uint32_t mag_count = 0;
    uint32_t tx_count = 0;
    uint32_t hr_count = 0;
    uint32_t hb_count = 0;
    uint32_t last_activity_classify = 0;

    while (1) {
        uint32_t now = k_uptime_get_32();

        /* IMU acquisition at 100Hz (10ms interval) */
        if (icm42688_read_accel(&imu_accel) == 0 &&
            icm42688_read_gyro(&imu_gyro) == 0) {

            /* Store in activity buffer */
            int idx = activity_buf.buf_index;
            activity_buf.accel_x[idx] = imu_accel.x;
            activity_buf.accel_y[idx] = imu_accel.y;
            activity_buf.accel_z[idx] = imu_accel.z;
            activity_buf.gyro_x[idx] = imu_gyro.x;
            activity_buf.gyro_y[idx] = imu_gyro.y;
            activity_buf.gyro_z[idx] = imu_gyro.z;

            activity_buf.buf_index = (idx + 1) % ACTIVITY_BUF_SIZE;
            if (activity_buf.buf_index == 0) activity_buf.buf_full = true;

            /* Magnetometer at 25Hz (every 4th IMU sample) */
            mag_count++;
            if (mag_count >= 4) {
                mmc5603_read(&imu_mag);
                mag_count = 0;
            }

            /* Madgwick filter update */
            madgwick_update(&madgwick,
                           imu_accel.x, imu_accel.y, imu_accel.z,
                           imu_gyro.x, imu_gyro.y, imu_gyro.z,
                           imu_mag.x, imu_mag.y, imu_mag.z);

            imu_count++;
        }

        /* BLE transmit at 2Hz (every 500ms) */
        tx_count++;
        if (tx_count >= 50) {  /* 50 × 10ms = 500ms */
            transmit_orientation();
            tx_count = 0;
        }

        /* Activity classification every 2s */
        if (now - last_activity_classify >= ACTIVITY_WINDOW_MS) {
            current_activity = classify_activity();
            last_activity_classify = now;
        }

        /* Heart rate sampling every 60s */
        hr_count++;
        if (hr_count >= 6000) {  /* 6000 × 10ms = 60s */
            gpio_pin_set(gpio_dev, 15, 1);  /* Enable MAX30101 */
            k_msleep(100);
            float hr, spo2;
            if (max30101_read_hr(&hr, &spo2) == 0) {
                heart_rate_bpm = hr;
                spo2_pct = spo2;
                transmit_heart_rate();
            }
            gpio_pin_set(gpio_dev, 15, 0);  /* Disable MAX30101 */
            hr_count = 0;
        }

        /* Heartbeat every 60s */
        hb_count++;
        if (hb_count >= 6000) {
            transmit_heartbeat();
            hb_count = 0;
        }

        /* Main loop at ~10ms (100Hz IMU) */
        k_msleep(10);
    }

    return 0;
}