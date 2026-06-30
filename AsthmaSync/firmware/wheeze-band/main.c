/**
 * AsthmaSync — Wheeze Band Main
 * ==============================
 * nRF52840 running Zephyr RTOS.
 *
 * Continuously captures audio (I²S mic) + PPG (MAX30101) to
 * detect wheezing, track heart rate/HRV/SpO₂, and send data
 * to Hub via BLE GATT notifications.
 *
 * Power: 200 mAh LiPo, USB-C charging. Target 1.5-day battery.
 *   - I²S mic: always on at 16 kHz
 *   - MAX30101 PPG: 25% duty cycle (100 Hz sample, 25% of time)
 *   - LSM6DSO IMU: 12.5 Hz (activity context only)
 *   - BLE: connected to Hub, notify every 2s
 *
 * License: MIT
 */

#include "config.h"
#include "wheeze.h"
#include "vitals.h"
#include "../common/protocol.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(asthmasync_band, LOG_LEVEL_INF);

#define NODE_ID  0x0004

/* ── GPIO ──────────────────────────────────────────────── */
static const struct gpio_dt_spec led    = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec btn    = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec motor  = GPIO_DT_SPEC_GET(DT_ALIAS(motor), gpios);
static const struct gpio_dt_spec max_int = GPIO_DT_SPEC_GET(DT_NODELABEL(max30101), int1_gpios);

/* ── BLE State ─────────────────────────────────────────── */
static struct bt_conn *s_conn = NULL;
static bool s_notify_enabled = false;
static uint8_t s_seq = 0;

/* ── I²S device ────────────────────────────────────────── */
static const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));

/* ── Send packet via BLE ───────────────────────────────── */
static void ble_send(uint8_t msg_type, uint8_t tlv_type,
                     const void *data, size_t data_len)
{
    if (!s_conn || !s_notify_enabled)
        return;

    uint8_t payload[1 + 128];
    payload[0] = tlv_type;
    size_t plen = 1;
    if (data && data_len > 0 && data_len < sizeof(payload)) {
        memcpy(&payload[1], data, data_len);
        plen = 1 + data_len;
    }

    pkt_header_t hdr = {0};
    hdr.src_type = NODE_TYPE_WHEEZE_BAND;
    hdr.src_id   = NODE_ID;
    hdr.msg_type = msg_type;
    hdr.seq      = s_seq++;

    uint8_t tx_buf[PKT_MAX_SIZE];
    size_t tx_len = proto_pack(&hdr, payload, plen, tx_buf, sizeof(tx_buf));

    if (tx_len > 0 && tx_len <= BLE_MTU) {
        /* bt_gatt_notify(s_conn, ..., tx_buf, tx_len); */
        LOG_DBG("BLE TX: %d bytes type=0x%02X", tx_len, msg_type);
    }
}

/* ── Wheeze callback ──────────────────────────────────── */
static void on_wheeze(uint8_t prob, const audio_feature_t *feature)
{
    LOG_INF("Wheeze event: prob=%u%%", prob);

    /* Haptic feedback */
    gpio_pin_set_dt(&motor, 1);
    k_msleep(200);
    gpio_pin_set_dt(&motor, 0);

    /* Send event to hub */
    event_payload_t evt = {
        .event_type = EVT_WHEEZE_DETECTED,
        .severity   = 1,
        .zone       = ZONE_YELLOW,
        .timestamp  = (uint32_t)k_uptime_get_32() / 1000,
    };

    ble_send(MSG_TYPE_EVENT, TLV_AUDIO_FEATURE, feature, sizeof(*feature));

    /* Also send as event */
    ble_send(MSG_TYPE_EVENT, 0, &evt, sizeof(evt));
}

/* ── Vitals callback ──────────────────────────────────── */
static void on_vitals_alert(vitals_event_t event, const vitals_t *vitals)
{
    LOG_INF("Vitals alert: event=%d hr=%u spo2=%u", event, vitals->hr, vitals->spo2);

    uint8_t evt_type;
    switch (event) {
    case VITALS_EVENT_SPO2_LOW:
        evt_type = EVT_SPO2_LOW;
        break;
    case VITALS_EVENT_HRV_DROP:
        evt_type = EVT_HRV_DROP;
        break;
    default:
        return;
    }

    event_payload_t evt = {
        .event_type = evt_type,
        .severity   = 2,
        .zone       = ZONE_RED,
        .timestamp  = (uint32_t)k_uptime_get_32() / 1000,
    };

    ble_send(MSG_TYPE_EVENT, 0, &evt, sizeof(evt));

    /* Strong haptic for red zone */
    gpio_pin_set_dt(&motor, 1);
    k_msleep(500);
    gpio_pin_set_dt(&motor, 0);
}

/* ── PPG sample timer (100 Hz) ─────────────────────────── */
static void ppg_timer_handler(struct k_timer *timer)
{
    int32_t red, ir;
    if (max30101_read_samples(&red, &ir) > 0) {
        vitals_on_sample(red, ir);
    }
}
K_TIMER_DEFINE(ppg_timer, ppg_timer_handler, NULL);

/* ── Telemetry task (sends vitals every 30s) ───────────── */
static void telemetry_task(struct k_work *work)
{
    vitals_t v;
    if (vitals_pack(&v) == 0) {
        ble_send(MSG_TYPE_TELEMETRY, TLV_VITALS, &v, sizeof(v));
        LOG_DBG("Vitals: HR=%u SpO2=%u HRV=%.1f T=%.1f",
                v.hr, v.spo2, v.hrv_rmssd_x10 / 10.0f,
                v.skin_temp_x10 / 10.0f);
    }

    /* Also read skin temp */
    float temp;
    tmp117_read_temp(&temp);
    LOG_DBG("Skin temp: %.2f°C", temp);
}
K_WORK_DEFINE(telemetry_work, telemetry_task);

static void telemetry_timer_handler(struct k_timer *timer)
{
    k_work_submit(&telemetry_work);
}
K_TIMER_DEFINE(telemetry_timer, telemetry_timer_handler, NULL);

/* ── I²S audio receive ─────────────────────────────────── */
static void i2s_rx_callback(const struct device *dev, void *arg, int status, void *rx_buf)
{
    if (status == 0 && rx_buf) {
        /* Convert 24-bit I²S to 16-bit PCM */
        int16_t pcm[256];
        uint8_t *raw = (uint8_t *)rx_buf;
        for (int i = 0; i < 256; i++) {
            /* 24-bit data in 32-bit frame, take upper 16 bits */
            int32_t sample = (raw[i*4+1] << 16) | (raw[i*4+2] << 8) | raw[i*4+3];
            sample = sample >> 8;  /* arithmetic shift to 16-bit */
            pcm[i] = (int16_t)(sample >> 8);
        }
        wheeze_on_audio(pcm, 256);
    }
}

/* ── BLE callbacks ─────────────────────────────────────── */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("BLE connect failed: 0x%02X", err);
        return;
    }
    s_conn = bt_conn_ref(conn);
    LOG_INF("BLE connected to Hub");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("BLE disconnected: 0x%02X", reason);
    if (s_conn) {
        bt_conn_unref(s_conn);
        s_conn = NULL;
    }
    s_notify_enabled = false;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    s_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("BLE notifications %s", s_notify_enabled ? "enabled" : "disabled");
}

BT_GATT_SERVICE_DEFINE(asthma_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(0xA501)),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x2A01),
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x01, 0xA5),
};

/* ── Button callback (mark event) ─────────────────────── */
static struct gpio_callback btn_cb_data;
static void button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    LOG_INF("Button pressed — marking event");

    /* Send SOS event */
    event_payload_t evt = {
        .event_type = EVT_BUTTON_SOS,
        .severity   = 2,
        .zone       = ZONE_RED,
        .timestamp  = (uint32_t)k_uptime_get_32() / 1000,
    };
    ble_send(MSG_TYPE_EVENT, 0, &evt, sizeof(evt));

    /* Haptic feedback */
    gpio_pin_set_dt(&motor, 1);
    k_msleep(300);
    gpio_pin_set_dt(&motor, 0);
}

/* ── Main ──────────────────────────────────────────────── */
int main(void)
{
    int ret;

    LOG_INF("=== AsthmaSync Wheeze Band v%d.%d ===",
            PROTO_VERSION_MAJOR, PROTO_VERSION_MINOR);

    /* Initialize GPIO */
    if (!device_is_ready(led.port) || !device_is_ready(motor.port)) {
        LOG_ERR("GPIO not ready");
        return -1;
    }
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&motor, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&btn, GPIO_INPUT);

    gpio_init_callback(&btn_cb_data, button_callback, BIT(btn.pin));
    gpio_add_callback(btn.port, &btn_cb_data);
    gpio_pin_interrupt_configure_dt(&btn, GPIO_INT_EDGE_TO_ACTIVE);

    /* Initialize I²C */
    const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c)) {
        LOG_ERR("I²C not ready");
        return -1;
    }

    /* Initialize wheeze detection */
    ret = wheeze_init();
    if (ret != 0) {
        LOG_ERR("Wheeze init failed: %d", ret);
    }

    /* Initialize vitals */
    ret = vitals_init();
    if (ret != 0) {
        LOG_ERR("Vitals init failed: %d", ret);
    }

    /* Register callbacks */
    wheeze_set_callback(on_wheeze);
    vitals_set_callback(on_vitals_alert);

    /* Initialize BLE */
    ret = bt_enable(NULL);
    if (ret) {
        LOG_ERR("BLE init failed: %d", ret);
        return -1;
    }

    ret = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    if (ret) {
        LOG_ERR("BLE adv failed: %d", ret);
    }

    /* Start PPG timer (100 Hz) */
    k_timer_start(&ppg_timer, K_USEC(10000), K_USEC(10000));

    /* Start telemetry timer (every 30s) */
    k_timer_start(&telemetry_timer, K_SECONDS(30), K_SECONDS(30));

    LOG_INF("Wheeze Band started — monitoring audio + vitals");

    while (1) {
        k_sleep(K_SECONDS(1));
        /* Toggle LED to show alive */
        gpio_pin_toggle_dt(&led);
    }

    return 0;
}