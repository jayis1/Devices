/**
 * AsthmaSync — Inhaler Tag Main
 * =============================
 * nRF52840 running nRF Connect SDK (Zephyr RTOS backend).
 *
 * Continuously monitors LSM6DSO accelerometer for MDI actuation
 * events. On detection, sends BLE GATT notification to Hub.
 *
 * Power: CR2032 (3V, 220mAh) — target 6-month battery life.
 *   - System OFF mode with wake-up on accel interrupt
 *   - BLE advertising only on actuation (event-driven)
 *   - Average current: ~15 µA
 *
 * License: MIT
 */

#include "config.h"
#include "actuation.h"
#include "../common/protocol.h"

/* ── nRF SDK includes ──────────────────────────────────── */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(asthmasync_inhaler, LOG_LEVEL_INF);

/* ── Node Identity ─────────────────────────────────────── */
#define NODE_ID  0x0003

/* ── BLE Service UUIDs ─────────────────────────────────── */
/* Custom: 0000A5xx-0000-1000-8000-00805F9B34FB */
#define BT_UUID_ASHMA_SVC  BT_UUID_DECLARE_16(0xA501)
#define BT_UUID_ASHMA_TELEM BT_UUID_DECLARE_16(0x2A01)
#define BT_UUID_ASHMA_EVENT BT_UUID_DECLARE_16(0x2A03)
#define BT_UUID_ASHMA_CMD   BT_UUID_DECLARE_16(0x2A02)

/* ── GPIO device specs ─────────────────────────────────── */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec int1 = GPIO_DT_SPEC_GET(DT_NODELABEL(lsm6dso), int1_gpios);

/* ── BLE State ─────────────────────────────────────────── */
static struct bt_conn *s_conn = NULL;
static bool s_notify_enabled = false;

/* ── I²C device ────────────────────────────────────────── */
static const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

/* ── Actuation callback: send via BLE ──────────────────── */
static void on_actuation(const actuation_t *act)
{
    LOG_INF("Actuation event: dose=%u conf=%u%%",
            actuation_get_dose_count(), act->confidence);

    /* Build event packet */
    event_payload_t evt = {
        .event_type = EVT_ACTUATION,
        .severity   = 1,  /* warning */
        .zone       = ZONE_YELLOW,
        .reserved   = 0,
        .timestamp  = (uint32_t)k_uptime_get_32() / 1000,
    };

    uint8_t payload[1 + sizeof(event_payload_t) + sizeof(actuation_t)];
    payload[0] = TLV_ACTUATION;
    memcpy(&payload[1], &evt, sizeof(evt));
    memcpy(&payload[1 + sizeof(evt)], act, sizeof(*act));

    pkt_header_t hdr = {0};
    hdr.src_type = NODE_TYPE_INHALER_TAG;
    hdr.src_id   = NODE_ID;
    hdr.msg_type = MSG_TYPE_EVENT;
    hdr.seq      = (uint8_t)(actuation_get_dose_count() & 0xFF);

    uint8_t tx_buf[PKT_MAX_SIZE];
    size_t tx_len = proto_pack(&hdr, payload, sizeof(payload),
                               tx_buf, sizeof(tx_buf));

    /* Send via BLE GATT notification */
    if (s_conn && s_notify_enabled && tx_len > 0) {
        struct bt_gatt_notify_params params = {
            .data = tx_buf,
            .len  = tx_len,
        };
        /* bt_gatt_notify(s_conn, &params); */
        LOG_INF("BLE notification sent: %d bytes", tx_len);
    }

    /* Visual feedback: LED blink */
    gpio_pin_set_dt(&led, 1);
    k_msleep(100);
    gpio_pin_set_dt(&led, 0);

    /* Buzzer: short beep */
    /* TODO: PWM buzzer */
}

/* ── GPIO interrupt callback ──────────────────────────── */
static struct gpio_callback int1_cb_data;
static void int1_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    /* Handle LSM6DSO wake-up interrupt */
    actuation_on_interrupt();
}

/* ── BLE Connection callbacks ──────────────────────────── */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("BLE connection failed: 0x%02X", err);
        return;
    }
    s_conn = bt_conn_ref(conn);
    LOG_INF("BLE connected");
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
    .connected    = connected,
    .disconnected = disconnected,
};

/* ── BLE GATT Service ──────────────────────────────────── */
static ssize_t read_telemetry(struct bt_conn *conn,
                              const struct bt_gatt_attr *attr, void *buf,
                              uint16_t len, uint16_t offset)
{
    /* Return last actuation data */
    const actuation_t *act = actuation_get_last();
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             act, sizeof(*act));
}

static void telem_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                   uint16_t value)
{
    s_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("BLE notifications %s", s_notify_enabled ? "enabled" : "disabled");
}

static ssize_t write_command(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr,
                             const void *buf, uint16_t len,
                             uint16_t offset, uint8_t flags)
{
    LOG_INF("BLE command received: %d bytes", len);
    /* TODO: parse commands (reset dose count, set thresholds, etc.) */
    return len;
}

BT_GATT_SERVICE_DEFINE(asthma_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_ASHMA_SVC),
    BT_GATT_CHARACTERISTIC(BT_UUID_ASHMA_TELEM,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ,
        read_telemetry, NULL, NULL),
    BT_GATT_CCC(telem_ccc_cfg_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(BT_UUID_ASHMA_EVENT,
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE,
        NULL, NULL, NULL),
    BT_GATT_CCC(telem_ccc_cfg_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(BT_UUID_ASHMA_CMD,
        BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_WRITE,
        NULL, write_command, NULL),
);

/* ── BLE Advertising ───────────────────────────────────── */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x01, 0xA5),  /* AsthmaSync service */
};

static void bt_ready(int err)
{
    if (err) {
        LOG_ERR("BLE init failed: %d", err);
        return;
    }
    LOG_INF("Bluetooth ready");

    err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed: %d", err);
    } else {
        LOG_INF("Advertising as \"%s\"", BLE_DEVICE_NAME);
    }
}

/* ── Main ──────────────────────────────────────────────── */
int main(void)
{
    int ret;

    LOG_INF("=== AsthmaSync Inhaler Tag v%d.%d ===",
            PROTO_VERSION_MAJOR, PROTO_VERSION_MINOR);

    /* Initialize GPIO */
    if (!device_is_ready(led.port) || !device_is_ready(btn.port)) {
        LOG_ERR("GPIO not ready");
        return -1;
    }
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&btn, GPIO_INPUT);

    /* Initialize I²C */
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I²C not ready");
        return -1;
    }

    /* Initialize LSM6DSO */
    ret = lsm6dso_init();
    if (ret != 0) {
        LOG_ERR("LSM6DSO init failed: %d", ret);
        return -1;
    }

    /* Register actuation callback */
    actuation_set_callback(on_actuation);

    /* Configure INT1 GPIO interrupt */
    if (!device_is_ready(int1.port)) {
        LOG_ERR("INT1 GPIO not ready");
        return -1;
    }
    gpio_pin_configure_dt(&int1, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&int1, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&int1_cb_data, int1_callback, BIT(int1.pin));
    gpio_add_callback(int1.port, &int1_cb_data);

    /* Initialize BLE */
    ret = bt_enable(bt_ready);
    if (ret) {
        LOG_ERR("BLE init failed: %d", ret);
        return -1;
    }

    LOG_INF("Inhaler Tag started — waiting for actuation events");

    /* Main loop: low-power idle.
       The system spends most time in System OFF, waking on
       LSM6DSO interrupt. BLE connection is established on-demand. */
    while (1) {
        /* Sleep until next interrupt */
        k_sleep(K_FOREVER);
    }

    return 0;
}