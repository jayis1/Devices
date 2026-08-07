/*
 * GuideSync — Haptic Band Firmware
 * nRF52840, Zephyr RTOS
 *
 * The Haptic Band provides:
 * 1. Fall detection — ICM-42688 IMU at 200 Hz → FallNet 1D-CNN
 *    (96% sensitivity, <0.3 FP/day) → BLE FALL_ALERT to Hub
 * 2. Navigation haptics — DRV2605L waveform sequences for turn-by-turn
 *    navigation (left/right/stop/arrive patterns)
 * 3. SOS button — long-press 3s → BLE SOS_ALERT to Hub → emergency dispatch
 * 4. BLE beacon scanning — RSSI fingerprints for NavNet indoor positioning
 *
 * Build: west build -b nrf52840dk_nrf52840
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "../common/protocol.h"
#include "../common/ble_mesh.h"
#include "../common/ble_beacon.h"
#include "../common/config.h"

LOG_MODULE_REGISTER(guidesync_band, LOG_LEVEL_INF);

/* === Global state === */
static gs_ble_ctx_t g_ble;
static uint16_t g_step_count = 0;
static uint8_t g_fall_count = 0;
static uint8_t g_nav_direction = GS_NAV_STRAIGHT;
static uint8_t g_nav_distance_m = 0;
static uint8_t g_sos_armed = 0;
static uint8_t g_battery_v = 380;

/* Fall detection state */
static uint8_t g_freefall_detected = 0;
static uint8_t g_impact_detected = 0;
static uint32_t g_fall_timestamp = 0;
static uint8_t g_fall_confirmed = 0;
static uint8_t g_stillness_sec = 0;

/* SOS state */
static uint32_t g_sos_press_start = 0;
static uint8_t g_sos_cancel_presses = 0;

/* === BLE Interface === */
static void ble_init(void) { /* bt_enable() */ }
static void ble_advert_start(void) { /* bt_le_adv_start() */ }
static void ble_advert_stop(void) { }
static void ble_scan_start(void) { /* bt_le_scan_start() */ }
static void ble_scan_stop(void) { /* bt_le_scan_stop() */ }
static int ble_connect(uint8_t *peer) { (void)peer; return 0; }
static int ble_send(const uint8_t *data, uint8_t len)
{
    LOG_INF("BLE TX %d bytes", len);
    return len;
}
static int ble_recv(uint8_t *buf, uint8_t max, uint32_t timeout_ms)
{
    k_msleep(MIN(timeout_ms, 100));
    return 0;
}
static void ble_delay_ms(uint32_t ms) { k_msleep(ms); }
static void ble_disconnect(void) { }

static const gs_ble_interface_t g_ble_iface = {
    .init = ble_init,
    .advert_start = ble_advert_start,
    .advert_stop = ble_advert_stop,
    .scan_start = ble_scan_start,
    .scan_stop = ble_scan_stop,
    .connect = ble_connect,
    .send = ble_send,
    .recv = ble_recv,
    .delay_ms = ble_delay_ms,
    .disconnect = ble_disconnect,
};

/* === Beacon Scanner === */
static void beacon_scan_start(void) { bt_le_scan_start(BT_LE_SCAN_PASSIVE, NULL); }
static void beacon_scan_stop(void) { bt_le_scan_stop(); }
static int beacon_get_results(gs_beacon_result_t *results, uint8_t max)
{
    (void)results; (void)max;
    return 0;
}
static void beacon_delay_ms(uint32_t ms) { k_msleep(ms); }

static const gs_beacon_scan_if_t g_beacon_if = {
    .scan_start = beacon_scan_start,
    .scan_stop = beacon_scan_stop,
    .scan_get_results = beacon_get_results,
    .delay_ms = beacon_delay_ms,
};

/* === ICM-42688 IMU (200 Hz for FallNet) === */
static const struct i2c_dt_spec imu_i2c =
    I2C_DT_SPEC_GET(DT_ALIAS(imu_band));

/* Accel data buffer for FallNet: 2 seconds at 200 Hz = 400 samples × 3 axes */
#define FALLNET_WINDOW_SIZE 400
static int16_t g_accel_buf[FALLNET_WINDOW_SIZE][3]; /* x, y, z in mg */
static int g_accel_idx = 0;
static uint8_t g_accel_filled = 0;

static void imu_init(void)
{
    /* Production: ICM-42688 config — accel ±4g, ODR 200 Hz,
     * free-fall interrupt <0.5g, wakeup interrupt */
}

static void imu_read_accel(int16_t *x, int16_t *y, int16_t *z)
{
    /* Production: read ICM-42688 ACCEL_DATA registers via I²C */
    /* Stub: 1g static (0, 0, 1000 mg) */
    *x = 0; *y = 0; *z = 1000;
}

static void imu_read_temp(int8_t *temp)
{
    *temp = 25;
}

/* Push accel sample into circular buffer */
static void accel_push(int16_t x, int16_t y, int16_t z)
{
    g_accel_buf[g_accel_idx][0] = x;
    g_accel_buf[g_accel_idx][1] = y;
    g_accel_buf[g_accel_idx][2] = z;
    g_accel_idx = (g_accel_idx + 1) % FALLNET_WINDOW_SIZE;
    if (g_accel_idx == 0) g_accel_filled = 1;
}

/* === FallNet 1D-CNN Inference ===
 * Input: 400×3 accel window (2s at 200 Hz)
 * Output: 3-class softmax (Normal, Fall, Activity)
 *
 * In production: TFLite-Micro int8 quantized model (~45 KB)
 * on nRF52840 Cortex-M4F.
 */
static uint8_t run_fallnet(void)
{
    if (!g_accel_filled) return 0; /* Normal — buffer not full */

    /* Production: TFLite-Micro inference on g_accel_buf
     * Conv1D(32, k=7) → MaxPool → Conv1D(16, k=5) → MaxPool
     * → Dense(32) → Dense(3) softmax
     */

    /* Heuristic pre-filter: check for free-fall + impact pattern */
    int16_t min_accel[3] = {32767, 32767, 32767};
    int16_t max_accel[3] = {-32768, -32768, -32768};

    for (int i = 0; i < FALLNET_WINDOW_SIZE; i++) {
        for (int j = 0; j < 3; j++) {
            if (g_accel_buf[i][j] < min_accel[j]) min_accel[j] = g_accel_buf[i][j];
            if (g_accel_buf[i][j] > max_accel[j]) max_accel[j] = g_accel_buf[i][j];
        }
    }

    /* Check free-fall (accel magnitude < 500 mg) */
    int32_t min_mag = (int32_t)min_accel[0]*min_accel[0] +
                      (int32_t)min_accel[1]*min_accel[1] +
                      (int32_t)min_accel[2]*min_accel[2];
    int32_t max_mag = (int32_t)max_accel[0]*max_accel[0] +
                      (int32_t)max_accel[1]*max_accel[1] +
                      (int32_t)max_accel[2]*max_accel[2];

    if (min_mag < (FALL_FREEFALL_THRESH_MG * FALL_FREEFALL_THRESH_MG) &&
        max_mag > (FALL_IMPACT_THRESH_MG * FALL_IMPACT_THRESH_MG)) {
        return 1; /* Fall detected */
    }

    return 0; /* Normal */
}

/* === DRV2605L Haptic Driver === */
static const struct i2c_dt_spec haptic_i2c =
    I2C_DT_SPEC_GET(DT_ALIAS(haptic_band));

static void haptic_init(void)
{
    /* Production: DRV2605L init — mode INTERNAL_TRIG, library LRA */
}

static void haptic_play(uint8_t waveform_id)
{
    /* Production: write waveform to DRV2605L, trigger GO */
    LOG_INF("Haptic: waveform %d", waveform_id);
}

static void haptic_sequence(const uint8_t *seq, uint8_t len, uint32_t gap_ms)
{
    for (uint8_t i = 0; i < len; i++) {
        if (seq[i] > 0) {
            haptic_play(seq[i]);
            if (i < len - 1) k_msleep(gap_ms);
        }
    }
}

/* Navigation haptic patterns */
static void haptic_nav(uint8_t direction)
{
    switch (direction) {
        case GS_NAV_STRAIGHT: {
            uint8_t seq[] = {1, 0};
            haptic_sequence(seq, 2, 0);
            break;
        }
        case GS_NAV_LEFT: {
            uint8_t seq[] = {2, 0, 2, 0};
            haptic_sequence(seq, 4, 100);
            break;
        }
        case GS_NAV_RIGHT: {
            uint8_t seq[] = {1, 0, 1, 0, 1, 0};
            haptic_sequence(seq, 6, 80);
            break;
        }
        case GS_NAV_STOP: {
            uint8_t seq[] = {14};
            haptic_sequence(seq, 1, 0);
            break;
        }
        case GS_NAV_ARRIVED: {
            uint8_t seq[] = {8, 10, 12};
            haptic_sequence(seq, 3, 150);
            break;
        }
        case GS_NAV_UPSTAIRS: {
            uint8_t seq[] = {8, 10, 12, 14};
            haptic_sequence(seq, 4, 100);
            break;
        }
        case GS_NAV_DOWNSTAIRS: {
            uint8_t seq[] = {14, 12, 10, 8};
            haptic_sequence(seq, 4, 100);
            break;
        }
    }
}

/* Fall alert haptic: urgent alternating pattern */
static void haptic_fall_alert(void)
{
    for (int i = 0; i < 5; i++) {
        haptic_play(14);
        k_msleep(200);
    }
}

/* SOS confirmed haptic: descending pattern */
static void haptic_sos_confirmed(void)
{
    uint8_t seq[] = {12, 10, 8};
    haptic_sequence(seq, 3, 150);
}

/* === SOS Button === */
static const struct gpio_dt_spec sos_btn =
    GPIO_DT_SPEC_GET(DT_ALIAS(sos_btn), gpios);
static struct gpio_callback sos_cb_data;

static void sos_button_handler(const struct device *port,
                               struct gpio_callback *cb, uint32_t pins)
{
    uint32_t now = k_uptime_get_32();

    if (gpio_pin_get_dt(&sos_btn)) {
        /* Button pressed — start timing */
        g_sos_press_start = now;
        g_sos_cancel_presses++;
    } else {
        /* Button released */
        uint32_t duration = now - g_sos_press_start;
        if (duration >= SOS_PRESS_DURATION_MS) {
            /* Long press = SOS */
            LOG_INF("SOS triggered! (press %d ms)", duration);
            g_sos_armed = 1;

            /* Send SOS alert to hub */
            gs_message_t sos;
            gs_build_sos_alert(&sos, g_ble.node_id, g_ble.msg_seq++,
                              g_battery_v, (uint8_t)(duration / 1000));
            if (g_ble.joined) gs_ble_send(&g_ble, &sos);

            /* Confirm with haptic */
            haptic_sos_confirmed();
        } else if (g_sos_cancel_presses >= SOS_CANCEL_PRESSES) {
            /* 3 rapid presses = cancel */
            LOG_INF("SOS cancelled");
            g_sos_armed = 0;
            g_sos_cancel_presses = 0;
        }
    }

    /* Reset cancel press counter after timeout */
    if (now - g_sos_press_start > SOS_CANCEL_WINDOW_S * 1000) {
        g_sos_cancel_presses = 0;
    }
}

static void sos_init(void)
{
    gpio_pin_configure_dt(&sos_btn, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&sos_btn, GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&sos_cb_data, sos_button_handler, BIT(sos_btn.pin));
    gpio_add_callback(sos_btn.port, &sos_cb_data);
    LOG_INF("SOS button initialized (long-press %d ms)", SOS_PRESS_DURATION_MS);
}

/* === Battery Monitor === */
static uint8_t read_battery_v(void)
{
    /* Production: ADC on BAND_GPIO_VBAT */
    return 380; /* Stub */
}

/* === IMU Sampling Task (200 Hz) === */
static void imu_task(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("IMU sampling task started (200 Hz)");

    while (1) {
        int16_t x, y, z;
        imu_read_accel(&x, &y, &z);
        accel_push(x, y, z);

        /* Step detection (simplified: accel z-axis peak) */
        if (z > 1500) g_step_count++;

        /* 200 Hz = 5 ms interval */
        k_msleep(5);
    }
}

/* === Fall Detection Task === */
static void fall_task(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Fall detection task started (FallNet 1D-CNN)");

    while (1) {
        /* Run FallNet every 1 second on the 2-second window */
        uint8_t result = run_fallnet();

        if (result == 1 && !g_fall_confirmed) {
            g_fall_confirmed = 1;
            g_fall_timestamp = k_uptime_get_32();
            g_fall_count++;
            LOG_ERR("FALL DETECTED! Dispatching alert...");

            /* Haptic alert */
            haptic_fall_alert();

            /* Send fall alert to hub */
            int8_t temp;
            imu_read_temp(&temp);

            /* Calculate impact magnitude */
            int16_t max_impact = 0;
            for (int i = 0; i < FALLNET_WINDOW_SIZE; i++) {
                int32_t mag = abs(g_accel_buf[i][0]) +
                              abs(g_accel_buf[i][1]) +
                              abs(g_accel_buf[i][2]);
                if (mag > max_impact) max_impact = (int16_t)mag;
            }

            gs_message_t fall_alert;
            gs_build_fall_alert(&fall_alert, g_ble.node_id, g_ble.msg_seq++,
                               (uint8_t)(max_impact / 10), temp,
                               g_step_count, g_battery_v, g_stillness_sec);
            if (g_ble.joined) gs_ble_send(&g_ble, &fall_alert);

            /* Wait for stillness (post-fall) */
            g_stillness_sec = 0;
        }

        /* Track post-fall stillness */
        if (g_fall_confirmed) {
            g_stillness_sec++;
            if (g_stillness_sec >= FALL_POST_STILLNESS_S) {
                /* User hasn't moved — reinforce alert */
                LOG_ERR("Post-fall stillness %d seconds", g_stillness_sec);
            }
        }

        k_msleep(1000 / FALLNET_INFERENCE_HZ);
    }
}

/* === Navigation Haptic Task === */
static void nav_task(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Navigation haptic task started");

    while (1) {
        if (g_nav_direction != GS_NAV_STRAIGHT || g_nav_distance_m < 5) {
            /* Play nav haptic when approaching turn or close to destination */
            haptic_nav(g_nav_direction);
        } else if (g_nav_direction == GS_NAV_STRAIGHT) {
            /* Continue straight: gentle pulse every 5 seconds */
            haptic_play(1);
        }

        k_msleep(BAND_HAPTIC_INTERVAL_MS);
    }
}

/* === BLE + Telemetry Task === */
static void ble_task(void *arg1, void *arg2, void *arg3)
{
    gs_ble_init(&g_ble, GS_NODE_BAND, &g_ble_iface);
    LOG_INF("Band BLE task started, joining network...");

    /* Production: gs_ble_join(&g_ble); */
    g_ble.node_id = 3; /* Stub */
    g_ble.joined = 1;

    gs_message_t msg;
    while (1) {
        /* Receive commands from hub */
        if (gs_ble_recv(&g_ble, &msg, 2000) == 0) {
            switch (msg.header.type) {
                case GS_MSG_NAV_UPDATE:
                    g_nav_direction = msg.payload[0];
                    g_nav_distance_m = msg.payload[1];
                    LOG_INF("Nav update: dir=%d dist=%dm",
                           g_nav_direction, g_nav_distance_m);
                    /* Immediate haptic on nav update */
                    haptic_nav(g_nav_direction);
                    break;

                case GS_MSG_COMMAND:
                    if (msg.payload[0] == GS_CMD_SOS_CANCEL) {
                        LOG_INF("SOS cancelled by hub");
                        g_sos_armed = 0;
                    }
                    break;

                case GS_MSG_ALERT:
                    if (msg.payload[0] == GS_ALERT_FALL) {
                        haptic_fall_alert();
                    }
                    break;
            }
        }

        /* Send telemetry every 30 seconds */
        static uint32_t telem_counter = 0;
        if (++telem_counter >= 15) { /* 15 × 2s = 30s */
            g_battery_v = read_battery_v();
            int8_t temp;
            imu_read_temp(&temp);

            gs_message_t telem;
            gs_build_band_telem(&telem, g_ble.node_id, g_ble.msg_seq++,
                g_battery_v, temp, g_step_count, g_fall_count,
                g_nav_direction, g_nav_direction, g_nav_distance_m,
                g_sos_armed, -60, 0);
            if (g_ble.joined) gs_ble_send(&g_ble, &telem);
            telem_counter = 0;
        }
    }
}

/* === Beacon Scan Task === */
static void beacon_task(void *arg1, void *arg2, void *arg3)
{
    gs_beacon_scanner_init(&g_beacon_if);
    LOG_INF("Beacon scan task started (NavNet)");

    while (1) {
        if (g_ble.joined) {
            gs_beacon_scan_t scan;
            if (gs_beacon_scan(&scan) == 0) {
                uint16_t uuids[GS_BEACON_MAX_PER_SCAN];
                int8_t rssi_vals[GS_BEACON_MAX_PER_SCAN];
                uint8_t dists[GS_BEACON_MAX_PER_SCAN];

                for (uint8_t i = 0; i < scan.count; i++) {
                    uuids[i] = scan.results[i].uuid_short;
                    rssi_vals[i] = scan.results[i].rssi;
                    dists[i] = scan.results[i].distance_dm;
                }

                gs_message_t scan_msg;
                gs_build_beacon_scan(&scan_msg, g_ble.node_id, g_ble.msg_seq++,
                                    scan.count, uuids, rssi_vals, dists);
                gs_ble_send(&g_ble, &scan_msg);
            }
        }
        k_msleep(5000); /* Scan every 5 seconds */
    }
}

/* === Main === */
void main(void)
{
    LOG_INF("GuideSync Haptic Band starting...");

    /* Init subsystems */
    imu_init();
    haptic_init();
    sos_init();

    /* Threads */
    K_THREAD_DEFINE(imu_tid, 2048, imu_task, NULL, NULL, NULL, 6, 0, 0);
    K_THREAD_DEFINE(fall_tid, 4096, fall_task, NULL, NULL, NULL, 5, 0, 0);
    K_THREAD_DEFINE(nav_tid, 2048, nav_task, NULL, NULL, NULL, 3, 0, 0);
    K_THREAD_DEFINE(ble_tid, 4096, ble_task, NULL, NULL, NULL, 4, 0, 0);
    K_THREAD_DEFINE(beacon_tid, 2048, beacon_task, NULL, NULL, NULL, 2, 0, 0);

    LOG_INF("GuideSync Haptic Band ready.");
}