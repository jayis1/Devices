/*
 * ErgoFlow — Hub Node Main
 * nRF5340 + ESP32-C6 based mesh coordinator and inference engine
 *
 * Responsibilities:
 *   - BLE mesh network coordinator (provisioner)
 *   - mmWave radar processing for pose detection
 *   - Local posture classification (TFLite Micro)
 *   - RSI risk assessment
 *   - Focus level detection
 *   - Ultradian rhythm break scheduling
 *   - Circadian lighting control
 *   - Audio playback for break reminders
 *   - OLED dashboard rendering
 *   - WiFi uplink via ESP32-C6 (MQTT)
 *   - BLE GATT server for mobile app
 *   - OTA update distribution
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include "common/ble_mesh/mesh_config.h"
#include "common/ble_mesh/mesh_handler.h"
#include "common/ble_mesh/protocol.h"
#include "common/sensors/i2c_bus.h"
#include "common/sensors/tsl2591.h"
#include "common/sensors/sht40.h"
#include "common/util/ringbuf.h"

LOG_MODULE_REGISTER(hub_main, CONFIG_ERGO_LOG_LEVEL);

/* ── Thread stack sizes ────────────────────────────────────────────── */
#define STACK_SIZE_MAIN      2048
#define STACK_SIZE_MESH      4096
#define STACK_SIZE_RADAR     4096
#define STACK_SIZE_INFERENCE 3072
#define STACK_SIZE_DISPLAY   1024
#define STACK_SIZE_AUDIO     1024
#define STACK_SIZE_UPLINK    2048

/* ── Thread priorities ──────────────────────────────────────────────── */
#define PRIO_MAIN       0
#define PRIO_MESH      -1
#define PRIO_RADAR      1
#define PRIO_INFERENCE  2
#define PRIO_DISPLAY    5
#define PRIO_AUDIO      5
#define PRIO_UPLINK     3

/* ── Sampling intervals ────────────────────────────────────────────── */
#define RADAR_SAMPLE_MS      50     /* 20 Hz radar */
#define AMBIENT_SAMPLE_MS    5000   /* 0.2 Hz ambient */
#define INFERENCE_MS         500    /* 2 Hz posture */
#define DISPLAY_UPDATE_MS    1000   /* 1 Hz display */
#define HEARTBEAT_MS         60000  /* 1 per minute */
#define UPLINK_MS            1000   /* 1 Hz MQTT */

/* ── Posture engine thresholds ─────────────────────────────────────── */
#define SLOUCH_THRESHOLD      40
#define LEAN_THRESHOLD         30
#define HUNCH_THRESHOLD        50
#define BREAK_INTERVAL_MIN     30    /* 30 min between breaks */
#define BREAK_DURATION_SEC     300   /* 5 min recommended break */
#define MAX_SIT_DURATION_MIN   60    /* Max continuous sitting */

/* ── Circadian lighting schedule ───────────────────────────────────── */
typedef struct {
    uint8_t hour;
    uint8_t r, g, b, w;
    uint8_t brightness;
} circadian_setpoint_t;

static const circadian_setpoint_t circadian_schedule[] = {
    { 6,  255, 200, 150, 200, 80  },  /* Morning: warm bright */
    { 8,  220, 230, 255, 180, 90  },  /* Work start: cool bright */
    { 10, 200, 220, 255, 150, 85  },  /* Focus: cool medium */
    { 12, 230, 230, 255, 170, 75  },  /* Midday: neutral */
    { 14, 220, 210, 240, 160, 70  },  /* Afternoon: slightly warm */
    { 16, 240, 200, 220, 140, 65  },  /* Late afternoon: warm */
    { 18, 255, 170, 130, 120, 50  },  /* Evening: warm dim */
    { 20, 200, 120, 80,  80,  30  },  /* Night: very warm dim */
    { 22, 120, 60,  40,  40,  15  },  /* Late night: red-ish */
};

#define CIRCADIAN_SETPOINTS (sizeof(circadian_schedule) / sizeof(circadian_setpoint_t))

/* ── Global state ──────────────────────────────────────────────────── */
typedef struct {
    /* Posture state */
    uint8_t posture_score;
    uint8_t posture_class;
    uint8_t rsi_risk;
    uint16_t sit_duration_s;
    uint16_t stand_duration_s;
    uint8_t focus_level;  /* 0=low, 1=medium, 2=high */

    /* Break state */
    uint32_t last_break_ts;
    uint32_t next_break_ts;
    uint8_t break_type;
    bool break_pending;

    /* Environment */
    uint32_t lux;
    int16_t temp_celsius;
    uint16_t humidity_pct;
    uint8_t lighting_r, lighting_g, lighting_b, lighting_w;
    uint8_t lighting_brightness;
    uint8_t lighting_mode;  /* 0=manual, 1=circadian */

    /* Network */
    bool chair_online;
    bool desk_online;
    bool tag1_online;
    bool tag2_online;
    uint8_t chair_batt;
    uint8_t desk_batt;
    uint8_t tag1_batt;
    uint8_t tag2_batt;

    /* System */
    uint8_t system_state;
    uint32_t uptime_s;
    uint8_t hub_batt_pct;
    bool usb_powered;
} hub_state_t;

static hub_state_t g_state = {
    .posture_score = 100,
    .posture_class = ERGO_POSTURE_GOOD,
    .rsi_risk = 0,
    .sit_duration_s = 0,
    .stand_duration_s = 0,
    .focus_level = 1,
    .break_pending = false,
    .lighting_mode = 1,  /* Default: circadian */
    .system_state = ERGO_STATE_INIT,
};

/* Ring buffers for sensor data */
static uint8_t pressure_buf_data[256];
static uint8_t imu_buf_data[512];
static ringbuf_t pressure_buf;
static ringbuf_t imu_buf;

/* ── Kthreads ──────────────────────────────────────────────────────── */
K_THREAD_STACK_DEFINE(mesh_stack, STACK_SIZE_MESH);
K_THREAD_STACK_DEFINE(radar_stack, STACK_SIZE_RADAR);
K_THREAD_STACK_DEFINE(inference_stack, STACK_SIZE_INFERENCE);
K_THREAD_STACK_DEFINE(display_stack, STACK_SIZE_DISPLAY);
K_THREAD_STACK_DEFINE(audio_stack, STACK_SIZE_AUDIO);
K_THREAD_STACK_DEFINE(uplink_stack, STACK_SIZE_UPLINK);

static struct k_thread mesh_thread, radar_thread, inference_thread;
static struct k_thread display_thread, audio_thread, uplink_thread;

/* ── Semaphores / Mutexes ──────────────────────────────────────────── */
static struct k_mutex state_mutex;
static struct k_sem radar_sem;
static struct k_sem inference_sem;
static struct k_sem break_sem;

/* ── Forward declarations ──────────────────────────────────────────── */
static void mesh_thread_fn(void *p1, void *p2, void *p3);
static void radar_thread_fn(void *p1, void *p2, void *p3);
static void inference_thread_fn(void *p1, void *p2, void *p3);
static void display_thread_fn(void *p1, void *p2, void *p3);
static void audio_thread_fn(void *p1, void *p2, void *p3);
static void uplink_thread_fn(void *p1, void *p2, void *p3);
static void update_circadian_lighting(void);
static void schedule_next_break(void);
static void send_break_reminder(uint8_t type, uint16_t duration);
static void send_lighting_command(void);
static void process_pressure_map(const uint8_t *data, uint16_t len, uint16_t src);
static void process_imu_orientation(const uint8_t *data, uint16_t len, uint16_t src);

/* ── Mesh message callback ─────────────────────────────────────────── */
static void mesh_msg_callback(uint16_t opcode, const uint8_t *data,
                               uint16_t len, uint16_t src_addr, void *user_data)
{
    switch (opcode) {
        case ERGO_OP_PRESSURE_MAP:
            process_pressure_map(data, len, src_addr);
            break;
        case ERGO_OP_IMU_ORIENTATION:
            process_imu_orientation(data, len, src_addr);
            break;
        case ERGO_OP_HEART_RATE:
            LOG_INF("Heart rate from 0x%04X: %d bpm, SpO2 %d%%",
                    src_addr, data[0], data[1]);
            break;
        case ERGO_OP_DESK_STATUS:
            LOG_INF("Desk status: height=%dmm, motor=%d, current=%dmA",
                    data[0] | (data[1] << 8), data[2], data[3] | (data[4] << 8));
            break;
        case ERGO_OP_NODE_HEARTBEAT: {
            ergo_node_heartbeat_t hb;
            if (ergo_unpack_node_heartbeat(data, len, &hb) == 0) {
                k_mutex_lock(&state_mutex, K_FOREVER);
                if (src_addr == ERGO_ADDR_CHAIR) {
                    g_state.chair_batt = hb.battery_pct;
                    g_state.chair_online = true;
                } else if (src_addr == ERGO_ADDR_DESK) {
                    g_state.desk_batt = hb.battery_pct;
                    g_state.desk_online = true;
                } else if (src_addr == ERGO_ADDR_TAG_1) {
                    g_state.tag1_batt = hb.battery_pct;
                    g_state.tag1_online = true;
                } else if (src_addr == ERGO_ADDR_TAG_2) {
                    g_state.tag2_batt = hb.battery_pct;
                    g_state.tag2_online = true;
                }
                k_mutex_unlock(&state_mutex);
                LOG_INF("Heartbeat from 0x%04X: batt=%d%%, state=%d, uptime=%dm",
                        src_addr, hb.battery_pct, hb.state, hb.uptime_min);
            }
            break;
        }
        default:
            LOG_DBG("Unhandled opcode 0x%04X from 0x%04X", opcode, src_addr);
            break;
    }
}

/* ── Process incoming pressure map ──────────────────────────────────── */
static void process_pressure_map(const uint8_t *data, uint16_t len, uint16_t src)
{
    if (src != ERGO_ADDR_CHAIR) return;

    ergo_pressure_map_t pm;
    if (ergo_unpack_pressure_map(data, len, &pm) != 0) return;

    /* Store in ring buffer for inference thread */
    ringbuf_put(&pressure_buf, data, len);
    k_sem_give(&inference_sem);

    /* Simple slouch detection (local, fast threshold check) */
    uint16_t back_total = 0, seat_total = 0;
    for (int i = 0; i < 8; i++) back_total += pm.pressure[i];
    for (int i = 8; i < 16; i++) seat_total += pm.pressure[i];

    /* Check if user is sitting */
    k_mutex_lock(&state_mutex, K_FOREVER);
    if (seat_total > 200) {
        g_state.sit_duration_s += (ERGO_TX_INTERVAL_PRESSURE / 1000);
        g_state.stand_duration_s = 0;

        /* Simple posture heuristics */
        uint8_t left_pressure = (pm.pressure[8] + pm.pressure[10]) / 2;
        uint8_t right_pressure = (pm.pressure[11] + pm.pressure[13]) / 2;
        uint8_t upper_back = (pm.pressure[0] + pm.pressure[1] + pm.pressure[2] + pm.pressure[3]) / 4;
        uint8_t lower_back = (pm.pressure[4] + pm.pressure[5] + pm.pressure[6] + pm.pressure[7]) / 4;

        if (upper_back > SLOUCH_THRESHOLD && lower_back < 10) {
            g_state.posture_class = ERGO_POSTURE_HUNCH;
            g_state.posture_score = 30;
        } else if (left_pressure > right_pressure + LEAN_THRESHOLD) {
            g_state.posture_class = ERGO_POSTURE_LEAN_LEFT;
            g_state.posture_score = 55;
        } else if (right_pressure > left_pressure + LEAN_THRESHOLD) {
            g_state.posture_class = ERGO_POSTURE_LEAN_RIGHT;
            g_state.posture_score = 55;
        } else if (lower_back > SLOUCH_THRESHOLD && upper_back < 10) {
            g_state.posture_class = ERGO_POSTURE_SLOUCH;
            g_state.posture_score = 40;
        } else {
            g_state.posture_class = ERGO_POSTURE_GOOD;
            g_state.posture_score = 85;
        }

        /* Check if sitting too long */
        if (g_state.sit_duration_s >= MAX_SIT_DURATION_MIN * 60 && !g_state.break_pending) {
            g_state.break_pending = true;
            g_state.break_type = ERGO_BREAK_WALK;
            k_sem_give(&break_sem);
        }
    } else {
        g_state.stand_duration_s += (ERGO_TX_INTERVAL_PRESSURE / 1000);
        g_state.sit_duration_s = 0;
        g_state.posture_class = ERGO_POSTURE_GOOD;
        g_state.posture_score = 95;
    }

    /* Update RSI risk based on posture history */
    g_state.rsi_risk = (g_state.rsi_risk * 9 + (100 - g_state.posture_score)) / 10;

    k_mutex_unlock(&state_mutex);
}

/* ── Process incoming IMU orientation ──────────────────────────────── */
static void process_imu_orientation(const uint8_t *data, uint16_t len, uint16_t src)
{
    if (src != ERGO_ADDR_TAG_1 && src != ERGO_ADDR_TAG_2) return;
    ringbuf_put(&imu_buf, data, len);
}

/* ── Circadian lighting ─────────────────────────────────────────────── */
static void update_circadian_lighting(void)
{
    /* Get current hour */
    uint32_t now = k_uptime_get_32() / 1000;
    uint8_t current_hour = (now / 3600) % 24;

    /* Find surrounding setpoints */
    int low_idx = 0, high_idx = 0;
    for (int i = 0; i < CIRCADIAN_SETPOINTS - 1; i++) {
        if (current_hour >= circadian_schedule[i].hour &&
            current_hour < circadian_schedule[i + 1].hour) {
            low_idx = i;
            high_idx = i + 1;
            break;
        }
    }

    /* Interpolate between setpoints */
    const circadian_setpoint_t *low = &circadian_schedule[low_idx];
    const circadian_setpoint_t *high = &circadian_schedule[high_idx];

    float t = 0.5f;
    if (high->hour != low->hour) {
        t = (float)(current_hour - low->hour) / (float)(high->hour - low->hour);
    }

    k_mutex_lock(&state_mutex, K_FOREVER);
    g_state.lighting_r = low->r + (uint8_t)((high->r - low->r) * t);
    g_state.lighting_g = low->g + (uint8_t)((high->g - low->g) * t);
    g_state.lighting_b = low->b + (uint8_t)((high->b - low->b) * t);
    g_state.lighting_w = low->w + (uint8_t)((high->w - low->w) * t);
    g_state.lighting_brightness = low->brightness + (uint8_t)((high->brightness - low->brightness) * t);
    k_mutex_unlock(&state_mutex);

    /* Send to desk controller */
    send_lighting_command();
}

/* ── Break scheduling ──────────────────────────────────────────────── */
static void schedule_next_break(void)
{
    k_mutex_lock(&state_mutex, K_FOREVER);
    uint32_t now = k_uptime_get_32() / 1000;
    g_state.next_break_ts = g_state.last_break_ts + (BREAK_INTERVAL_MIN * 60);
    k_mutex_unlock(&state_mutex);
}

static void send_break_reminder(uint8_t type, uint16_t duration)
{
    ergo_break_reminder_t reminder = {
        .type = type,
        .duration_s = duration,
    };
    uint8_t buf[8];
    uint16_t len;
    ergo_pack_break_reminder(&reminder, buf, &len);
    mesh_handler_send(ERGO_OP_BREAK_REMINDER, buf, len, 0xFFFF); /* Broadcast */
    LOG_INF("Break reminder sent: type=%d, duration=%ds", type, duration);
}

static void send_lighting_command(void)
{
    ergo_lighting_cmd_t cmd;
    k_mutex_lock(&state_mutex, K_FOREVER);
    cmd.r = g_state.lighting_r;
    cmd.g = g_state.lighting_g;
    cmd.b = g_state.lighting_b;
    cmd.w = g_state.lighting_w;
    cmd.brightness_pct = g_state.lighting_brightness;
    cmd.mode = g_state.lighting_mode;
    k_mutex_unlock(&state_mutex);

    uint8_t buf[8];
    uint16_t len;
    ergo_pack_lighting_cmd(&cmd, buf, &len);
    mesh_handler_send(ERGO_OP_LIGHTING_CMD, buf, len, ERGO_ADDR_DESK);
}

/* ── Mesh thread ───────────────────────────────────────────────────── */
static void mesh_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("Mesh thread started");
    mesh_handler_init();
    mesh_handler_register_callback(0xFFFF, mesh_msg_callback, NULL);

    while (1) {
        /* Mesh stack handles messages asynchronously */
        k_msleep(100);
    }
}

/* ── Radar processing thread ───────────────────────────────────────── */
static void radar_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("Radar thread started");

    while (1) {
        k_sem_take(&radar_sem, K_MSEC(RADAR_SAMPLE_MS));

        /* In production: read BGT60TR13C via SPI, process range-Doppler
         * For now: placeholder for radar data acquisition and processing */
    }
}

/* ── Inference thread ──────────────────────────────────────────────── */
static void inference_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("Inference thread started");

    while (1) {
        k_sem_take(&inference_sem, K_MSEC(INFERENCE_MS));

        /* In production: run TFLite Micro PostureNet model
         * For now: posture is computed locally in pressure map handler */

        /* Broadcast posture score to all nodes */
        ergo_posture_score_t score;
        k_mutex_lock(&state_mutex, K_FOREVER);
        score.score = g_state.posture_score;
        score.risk_level = g_state.rsi_risk > 60 ? 2 : (g_state.rsi_risk > 30 ? 1 : 0);
        score.duration_s = g_state.sit_duration_s;
        k_mutex_unlock(&state_mutex);

        uint8_t buf[8];
        uint16_t len;
        ergo_pack_posture_score(&score, buf, &len);
        mesh_handler_send(ERGO_OP_POSTURE_SCORE, buf, len, 0xFFFF);
    }
}

/* ── Display thread ─────────────────────────────────────────────────── */
static void display_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("Display thread started");

    while (1) {
        k_msleep(DISPLAY_UPDATE_MS);

        /* In production: render OLED dashboard
         * Shows: posture score, break timer, desk height, battery levels */
        k_mutex_lock(&state_mutex, K_FOREVER);
        uint8_t score = g_state.posture_score;
        uint8_t risk = g_state.rsi_risk;
        k_mutex_unlock(&state_mutex);

        LOG_INF("Display: posture=%d/100, risk=%d", score, risk);
    }
}

/* ── Audio thread ──────────────────────────────────────────────────── */
static void audio_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("Audio thread started");

    while (1) {
        k_sem_take(&break_sem, K_FOREVER);

        /* In production: play break reminder audio via MAX98357A
         * For now: log the break event */
        k_mutex_lock(&state_mutex, K_FOREVER);
        uint8_t type = g_state.break_type;
        k_mutex_unlock(&state_mutex);

        send_break_reminder(type, BREAK_DURATION_SEC);
    }
}

/* ── Uplink thread (ESP32-C6 UART bridge) ───────────────────────────── */
static void uplink_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("Uplink thread started");

    while (1) {
        k_msleep(UPLINK_MS);

        /* In production: send posture data to cloud via ESP32-C6 UART bridge
         * Format: ERGO_UART protocol with sync bytes, opcode, payload, CRC */

        /* Send ambient readings */
        ergo_ambient_reading_t ambient;
        k_mutex_lock(&state_mutex, K_FOREVER);
        ambient.lux = g_state.lux;
        ambient.temp_celsius = g_state.temp_celsius;
        ambient.humidity_pct = g_state.humidity_pct;
        k_mutex_unlock(&state_mutex);

        uint8_t buf[16];
        uint16_t len;
        ergo_pack_ambient_reading(&ambient, buf, &len);

        /* In production: esp32_bridge_send(OPCODE_AMBIENT, buf, len) */
    }
}

/* ── Main ───────────────────────────────────────────────────────────── */
int main(void)
{
    LOG_INF("ErgoFlow Hub Node starting...");

    /* Initialize mutexes and semaphores */
    k_mutex_init(&state_mutex);
    k_sem_init(&radar_sem, 0, 1);
    k_sem_init(&inference_sem, 0, 1);
    k_sem_init(&break_sem, 0, 1);

    /* Initialize ring buffers */
    ringbuf_init(&pressure_buf, pressure_buf_data, sizeof(pressure_buf_data));
    ringbuf_init(&imu_buf, imu_buf_data, sizeof(imu_buf_data));

    /* Initialize I2C bus */
    if (i2c_bus_init() != 0) {
        LOG_ERR("I2C bus init failed");
    } else {
        LOG_INF("I2C bus initialized");
        i2c_bus_scan();
    }

    /* Initialize sensors */
    if (tsl2591_init(TSL2591_ADDR) != 0) {
        LOG_WRN("TSL2591 ambient light sensor not found");
    }
    if (sht40_init(SHT40_ADDR) != 0) {
        LOG_WRN("SHT40 temp/humidity sensor not found");
    }

    /* Start threads */
    k_thread_create(&mesh_thread, mesh_stack, STACK_SIZE_MESH,
                    mesh_thread_fn, NULL, NULL, NULL, PRIO_MESH, 0, K_NO_WAIT);
    k_thread_create(&radar_thread, radar_stack, STACK_SIZE_RADAR,
                    radar_thread_fn, NULL, NULL, NULL, PRIO_RADAR, 0, K_NO_WAIT);
    k_thread_create(&inference_thread, inference_stack, STACK_SIZE_INFERENCE,
                    inference_thread_fn, NULL, NULL, NULL, PRIO_INFERENCE, 0, K_NO_WAIT);
    k_thread_create(&display_thread, display_stack, STACK_SIZE_DISPLAY,
                    display_thread_fn, NULL, NULL, NULL, PRIO_DISPLAY, 0, K_NO_WAIT);
    k_thread_create(&audio_thread, audio_stack, STACK_SIZE_AUDIO,
                    audio_thread_fn, NULL, NULL, NULL, PRIO_AUDIO, 0, K_NO_WAIT);
    k_thread_create(&uplink_thread, uplink_stack, STACK_SIZE_UPLINK,
                    uplink_thread_fn, NULL, NULL, NULL, PRIO_UPLINK, K_NO_WAIT);

    LOG_INF("All threads started, system running");

    /* Main loop: sample ambient sensors */
    g_state.system_state = ERGO_STATE_RUNNING;
    while (1) {
        /* Read ambient light */
        float lux;
        if (tsl2591_read_lux(&lux) == 0) {
            k_mutex_lock(&state_mutex, K_FOREVER);
            g_state.lux = (uint32_t)lux;
            k_mutex_unlock(&state_mutex);
        }

        /* Read temperature and humidity */
        float temp, humidity;
        if (sht40_read(&temp, &humidity) == 0) {
            k_mutex_lock(&state_mutex, K_FOREVER);
            g_state.temp_celsius = (int16_t)(temp * 10);
            g_state.humidity_pct = (uint16_t)(humidity * 10);
            k_mutex_unlock(&state_mutex);
        }

        /* Update circadian lighting every 5 minutes */
        if (g_state.lighting_mode == 1) {
            update_circadian_lighting();
        }

        /* Check break schedule */
        k_mutex_lock(&state_mutex, K_FOREVER);
        uint32_t now = k_uptime_get_32() / 1000;
        if (!g_state.break_pending &&
            (now - g_state.last_break_ts) >= (BREAK_INTERVAL_MIN * 60)) {
            /* Determine break type based on posture and duration */
            if (g_state.sit_duration_s > 45 * 60) {
                g_state.break_type = ERGO_BREAK_WALK;
            } else if (g_state.posture_score < 50) {
                g_state.break_type = ERGO_BREAK_STRETCH;
            } else {
                g_state.break_type = ERGO_BREAK_LOOK_AWAY;
            }
            g_state.break_pending = true;
            k_sem_give(&break_sem);
        }
        k_mutex_unlock(&state_mutex);

        k_msleep(AMBIENT_SAMPLE_MS);
    }

    return 0;
}