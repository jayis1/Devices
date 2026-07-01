/**
 * MigraineSync — Aura Band Main
 * ==============================
 * nRF52840 running Zephyr RTOS.
 *
 * Continuously captures PPG (MAX30101) for HR/HRV/SpO₂,
 * barometric pressure (BMP390), ambient light (VEML7700),
 * and activity (LSM6DSO). Sends fused data to Hub via BLE
 * GATT notifications every 5 seconds.
 *
 * Power: 200 mAh LiPo, USB-C charging. Target 1.5-day battery.
 *   - MAX30101 PPG: 25% duty cycle (100 Hz sample, 25% of time)
 *   - BMP390: 1 Hz ultra-low-power
 *   - VEML7700: 0.5 Hz
 *   - LSM6DSO: 0.5 Hz
 *   - BLE: connected to Hub, notify every 5s
 *
 * License: MIT
 */

#include "config.h"
#include "vitals.h"
#include "baro.h"
#include "light.h"
#include "../common/protocol.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(migrainesync_aura, LOG_LEVEL_INF);

/* ── GPIO ──────────────────────────────────────────────── */
static const struct gpio_dt_spec led    = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec btn    = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec motor  = GPIO_DT_SPEC_GET(DT_ALIAS(motor), gpios);

/* ── BLE State ─────────────────────────────────────────── */
static struct bt_conn *s_conn = NULL;
static bool s_notify_enabled = false;
static uint8_t s_seq = 0;

/* ── Vibrator (haptic alert) ────────────────────────────── */
static void vibrate(uint16_t duration_ms, uint8_t pulses)
{
    for (uint8_t i = 0; i < pulses; i++) {
        gpio_pin_set_dt(&motor, 1);
        k_msleep(duration_ms);
        gpio_pin_set_dt(&motor, 0);
        if (i < pulses - 1)
            k_msleep(duration_ms);
    }
}

/* ── BLE GATT: TX characteristic (notify) ───────────────── */
#define BLE_SERVICE_UUID_VAL  BT_UUID_128_ENCODE(0x6e400001, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)
#define BLE_TX_CHAR_UUID_VAL  BT_UUID_128_ENCODE(0x6e400002, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)

static struct bt_uuid_128 migrainesync_svc_uuid =
    BT_UUID_INIT_128(BLE_SERVICE_UUID_VAL);
static struct bt_uuid_128 tx_char_uuid =
    BT_UUID_INIT_128(BLE_TX_CHAR_UUID_VAL);

static void tx_ccc_cfg_changed(const struct bt_uuid *uuid, uint16_t attr_value)
{
    s_notify_enabled = (attr_value == BT_GATT_CCC_NOTIFY);
    LOG_INF("BLE notify %s", s_notify_enabled ? "enabled" : "disabled");
}

/* GATT service definition (simplified) */
static struct bt_gatt_attr migrainesync_attrs[] = {
    BT_GATT_PRIMARY_SERVICE(&migrainesync_svc_uuid),
    BT_GATT_CHARACTERISTIC(&tx_char_uuid.uuid,
                           BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           NULL, NULL, NULL),
    BT_GATT_CCC(tx_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
};

static struct bt_gatt_service migrainesync_svc =
    BT_GATT_SERVICE(migrainesync_attrs);

/* ── BLE callbacks ──────────────────────────────────────── */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_WRN("BLE connect failed (err %u)", err);
        return;
    }
    s_conn = bt_conn_ref(conn);
    LOG_INF("BLE connected to Hub");
    gpio_pin_set_dt(&led, 1);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("BLE disconnected (reason %u)", reason);
    if (s_conn) {
        bt_conn_unref(s_conn);
        s_conn = NULL;
    }
    s_notify_enabled = false;
    gpio_pin_set_dt(&led, 0);
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

/* ── Send data via BLE ──────────────────────────────────── */
static void ble_send_frame(const frame_t *frame)
{
    if (!s_conn || !s_notify_enabled)
        return;

    /* Send frame in 20-byte chunks (standard BLE MTU) */
    size_t total = 4 + frame->len + 2;
    const uint8_t *buf = (const uint8_t *)frame;

    for (size_t offset = 0; offset < total; offset += 20) {
        size_t chunk = (total - offset > 20) ? 20 : (total - offset);
        bt_gatt_notify(NULL, &migrainesync_attrs[1], &buf[offset], chunk);
    }
}

/* ── Build and send vitals + baro + light TLV frame ─────── */
static void send_telemetry(const vitals_t *v, float pressure,
                           float p_delta, float light_lux)
{
    uint8_t vitals_val[11];
    vitals_val[0] = v->hr_bpm;
    encode_f32(&vitals_val[1], v->hrv_rmssd);
    vitals_val[5] = v->spo2_pct;
    encode_f32(&vitals_val[6], v->skin_temp_c);
    vitals_val[10] = v->activity;

    uint8_t baro_val[12];
    encode_f32(&baro_val[0], pressure);
    encode_f32(&baro_val[4], p_delta);
    encode_f32(&baro_val[8], v->skin_temp_c);  /* reuse skin temp */

    uint8_t light_val[8];
    encode_f32(&light_val[0], light_lux);
    encode_f32(&light_val[4], light_lux);  /* cumulative (simplified) */

    /* Battery (simplified) */
    uint8_t batt_val[3];
    batt_val[0] = 85;  /* 85% (read ADC in production) */
    encode_u16(&batt_val[1], 3900);  /* 3.9V */

    tlv_t tlvs[4] = {
        { MSG_VITALS,     11, vitals_val },
        { MSG_BAROMETRIC, 12, baro_val   },
        { MSG_LIGHT_DOSE,  8, light_val  },
        { MSG_BATTERY,     3, batt_val   },
    };

    frame_t frame;
    size_t total = frame_build(&frame, s_seq++, tlvs, 4);
    if (total > 0)
        ble_send_frame(&frame);
}

/* ── Pressure delta tracking ────────────────────────────── */
#define PRESSURE_HISTORY_LEN  180   /* 3 min at 1 Hz → will aggregate for 3h */
static float s_pressure_history[180];
static int s_pressure_idx = 0;

static float compute_pressure_delta(float current)
{
    s_pressure_history[s_pressure_idx] = current;
    s_pressure_idx = (s_pressure_idx + 1) % PRESSURE_HISTORY_LEN;

    /* Simplified: compare current to oldest in buffer */
    int oldest = s_pressure_idx;  /* next write position = oldest */
    return current - s_pressure_history[oldest];
}

/* ── Button callback (mark prodrome/migraine) ───────────── */
static struct gpio_callback btn_cb_data;
static void button_pressed(const struct device *dev, struct gpio_callback *cb,
                           uint32_t pins)
{
    LOG_INF("Button pressed — marking prodrome/migraine event");

    /* Send MANUAL_EVENT to hub */
    uint8_t event_val[6];
    event_val[0] = 0;  /* migraine_onset */
    uint32_t ts = k_uptime_get() / 1000;  /* simplified timestamp */
    event_val[1] = ts & 0xFF;
    event_val[2] = (ts >> 8) & 0xFF;
    event_val[3] = (ts >> 16) & 0xFF;
    event_val[4] = (ts >> 24) & 0xFF;
    event_val[5] = 0;  /* no note */

    tlv_t tlv = { MSG_MANUAL_EVENT, 6, event_val };
    frame_t frame;
    size_t total = frame_build(&frame, s_seq++, &tlv, 1);
    if (total > 0)
        ble_send_frame(&frame);

    vibrate(200, 3);  /* confirm event logged */
}

/* ── Main ───────────────────────────────────────────────── */
int main(void)
{
    LOG_INF("MigraineSync Aura Band v%s starting...", FIRMWARE_VERSION);

    /* Init GPIO */
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&motor, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&btn, GPIO_INPUT);

    gpio_init_callback(&btn_cb_data, button_pressed, BIT(btn.pin));
    gpio_add_callback(btn.port, &btn_cb_data);
    gpio_pin_interrupt_configure_dt(&btn, GPIO_INT_EDGE_TO_ACTIVE);

    /* Init sensors */
    vitals_init();
    baro_init();
    light_init();

    /* Init BLE */
    bt_enable(NULL);
    bt_conn_cb_register(&conn_callbacks);
    bt_gatt_service_register(&migrainesync_svc);

    /* Start advertising */
    struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(
        BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_NAME,
        BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL);
    bt_le_adv_start(&adv_param, NULL, 0, NULL, 0);

    LOG_INF("BLE advertising started");

    /* ── Main loop ─────────────────────────────────────── */
    vitals_t vitals;
    float pressure, temp, p_delta, light_lux;

    while (1) {
        /* Read vitals (HR, HRV, SpO₂, skin temp, activity) */
        vitals_read(&vitals);

        /* Read barometric pressure */
        baro_read(&pressure, &temp);
        p_delta = compute_pressure_delta(pressure);

        /* Read ambient light */
        light_read(&light_lux);

        LOG_INF("HR=%u HRV=%.1f SpO2=%u skinT=%.1f P=%.1f ΔP=%.2f lux=%.0f act=%u",
                vitals.hr_bpm, vitals.hrv_rmssd, vitals.spo2_pct,
                vitals.skin_temp_c, pressure, p_delta, light_lux,
                vitals.activity);

        /* Send telemetry via BLE */
        send_telemetry(&vitals, pressure, p_delta, light_lux);

        k_sleep(K_SECONDS(BLE_TX_INTERVAL_S));
    }

    return 0;
}