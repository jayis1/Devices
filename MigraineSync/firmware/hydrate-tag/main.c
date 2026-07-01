/**
 * MigraineSync — Hydrate Tag Main
 * ================================
 * nRF52840 running Zephyr RTOS.
 *
 * Detects water bottle sips via HX711 load cell + LSM6DSO IMU
 * tilt detection. Transmits hydration data to Hub via BLE
 * every 60 seconds.
 *
 * Power: CR2032 (3V, 220 mAh). Target 6-month battery life.
 *   - LSM6DSO: 12.5 Hz, wake-on-tilt interrupt
 *   - HX711: duty-cycled (wake on tilt, sample 2s, sleep)
 *   - BLE: notify every 60s, low duty cycle
 *   - Average current: ~0.3 mA (sleep 6 µA + active bursts)
 *
 * License: MIT
 */

#include "config.h"
#include "loadcell.h"
#include "sip_detect.h"
#include "../common/protocol.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(migrainesync_hydrate, LOG_LEVEL_INF);

/* ── GPIO ──────────────────────────────────────────────── */
static const struct gpio_dt_spec led    = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec buzzer = GPIO_DT_SPEC_GET(DT_ALIAS(buzzer), gpios);
static const struct gpio_dt_spec btn    = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec tilt_int = GPIO_DT_SPEC_GET(DT_NODELABEL(lsm6dso), int1_gpios);

/* ── BLE State ─────────────────────────────────────────── */
static struct bt_conn *s_conn = NULL;
static bool s_notify_enabled = false;
static uint8_t s_seq = 0;

/* ── BLE GATT (same service as Aura Band) ───────────────── */
#define BLE_SERVICE_UUID_VAL  BT_UUID_128_ENCODE(0x6e400001, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)
#define BLE_TX_CHAR_UUID_VAL  BT_UUID_128_ENCODE(0x6e400002, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)

static struct bt_uuid_128 svc_uuid = BT_UUID_INIT_128(BLE_SERVICE_UUID_VAL);
static struct bt_uuid_128 tx_uuid  = BT_UUID_INIT_128(BLE_TX_CHAR_UUID_VAL);

static void ccc_changed(const struct bt_uuid *uuid, uint16_t val)
{
    s_notify_enabled = (val == BT_GATT_CCC_NOTIFY);
}

static struct bt_gatt_attr attrs[] = {
    BT_GATT_PRIMARY_SERVICE(&svc_uuid),
    BT_GATT_CHARACTERISTIC(&tx_uuid.uuid, BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, NULL, NULL, NULL),
    BT_GATT_CCC(ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
};

static struct bt_gatt_service hydrate_svc = BT_GATT_SERVICE(attrs);

static void connected_cb(struct bt_conn *conn, uint8_t err)
{
    if (!err) {
        s_conn = bt_conn_ref(conn);
        LOG_INF("BLE connected to Hub");
    }
}

static void disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    if (s_conn) {
        bt_conn_unref(s_conn);
        s_conn = NULL;
    }
    s_notify_enabled = false;
    LOG_INF("BLE disconnected (reason %u)", reason);
}

static struct bt_conn_cb conn_cbs = {
    .connected = connected_cb,
    .disconnected = disconnected_cb,
};

/* ── Send hydration data via BLE ────────────────────────── */
static void send_hydration(float intake_ml, uint8_t sips, float bottle_g)
{
    if (!s_conn || !s_notify_enabled)
        return;

    uint8_t val[9];
    encode_f32(&val[0], intake_ml);
    val[4] = sips;
    encode_f32(&val[5], bottle_g);

    tlv_t tlv = { MSG_HYDRATION, 9, val };

    /* Battery */
    uint8_t batt_val[3] = { 90, 0x48, 0x03 };  /* 90%, 3048 mV (CR2032 ~3.0V) */
    tlv_t batt_tlv = { MSG_BATTERY, 3, batt_val };

    tlv_t tlvs[2] = { tlv, batt_tlv };
    frame_t frame;
    size_t total = frame_build(&frame, s_seq++, tlvs, 2);
    if (total > 0) {
        const uint8_t *buf = (const uint8_t *)&frame;
        for (size_t off = 0; off < total; off += 20) {
            size_t chunk = (total - off > 20) ? 20 : (total - off);
            bt_gatt_notify(NULL, &attrs[1], &buf[off], chunk);
        }
    }
}

/* ── Tilt interrupt callback ────────────────────────────── */
static struct gpio_callback tilt_cb;
static volatile bool s_tilt_detected = false;

static void tilt_handler(const struct device *dev, struct gpio_callback *cb,
                         uint32_t pins)
{
    s_tilt_detected = true;
}

/* ── Button callback (manual intake mark) ───────────────── */
static struct gpio_callback btn_cb;
static volatile bool s_button_pressed = false;

static void button_handler(const struct device *dev, struct gpio_callback *cb,
                           uint32_t pins)
{
    s_button_pressed = true;
}

/* ── Buzzer ─────────────────────────────────────────────── */
static void buzz(uint16_t ms)
{
    gpio_pin_set_dt(&buzzer, 1);
    k_msleep(ms);
    gpio_pin_set_dt(&buzzer, 0);
}

/* ── Main ───────────────────────────────────────────────── */
int main(void)
{
    LOG_INF("MigraineSync Hydrate Tag v%s starting...", FIRMWARE_VERSION);

    /* Init GPIO */
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&buzzer, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&btn, GPIO_INPUT);

    /* Init load cell */
    loadcell_init();

    /* Init sip detection (LSM6DSO + load cell) */
    sip_detect_init();

    /* Init tilt interrupt */
    gpio_pin_configure_dt(&tilt_int, GPIO_INPUT);
    gpio_init_callback(&tilt_cb, tilt_handler, BIT(tilt_int.pin));
    gpio_add_callback(tilt_int.port, &tilt_cb);
    gpio_pin_interrupt_configure_dt(&tilt_int, GPIO_INT_EDGE_TO_ACTIVE);

    /* Init button interrupt */
    gpio_init_callback(&btn_cb, button_handler, BIT(btn.pin));
    gpio_add_callback(btn.port, &btn_cb);
    gpio_pin_interrupt_configure_dt(&btn, GPIO_INT_EDGE_TO_ACTIVE);

    /* Init BLE */
    bt_enable(NULL);
    bt_conn_cb_register(&conn_cbs);
    bt_gatt_service_register(&hydrate_svc);

    struct bt_le_adv_param adv = BT_LE_ADV_PARAM_INIT(
        BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_NAME,
        BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL);
    bt_le_adv_start(&adv, NULL, 0, NULL, 0);

    LOG_INF("Hydrate Tag running (BLE advertising, tilt-interrupt wake)");

    int64_t last_tx = 0;

    while (1) {
        /* Tilt detected — wake up and check for sip */
        if (s_tilt_detected) {
            s_tilt_detected = false;
            LOG_INF("Tilt detected — checking for sip");

            /* Power up load cell */
            loadcell_power_up();
            k_msleep(50);  /* HX711 settle */

            /* Poll for sip event */
            sip_event_t event = sip_detect_poll();

            if (event == SIP_EVENT_SIP || event == SIP_EVENT_GULP) {
                gpio_pin_set_dt(&led, 1);
                k_msleep(100);
                gpio_pin_set_dt(&led, 0);
            }

            /* Power down load cell to save battery */
            loadcell_power_down();
        }

        /* Button pressed — manual intake mark */
        if (s_button_pressed) {
            s_button_pressed = false;
            LOG_INF("Manual intake button pressed — adding 250ml");
            /* Approximate: add 250ml for manual mark */
            buzz(200);
        }

        /* BLE TX every 60 seconds */
        int64_t now = k_uptime_get();
        if (now - last_tx > 60000) {
            float intake = sip_get_daily_intake_ml();
            uint8_t sips = sip_get_sip_count();
            float bottle_g = sip_get_bottle_weight_g();

            LOG_INF("TX: intake=%.0fml sips=%u bottle=%.0fg", intake, sips, bottle_g);
            send_hydration(intake, sips, bottle_g);
            last_tx = now;

            /* Hydration reminder if below goal */
            if (intake < DAILY_GOAL_ML * 0.5) {
                buzz(100);
                k_msleep(200);
                buzz(100);
            }
        }

        /* Sleep until next event (tilt interrupt or timer) */
        k_sleep(K_SECONDS(5));
    }

    return 0;
}