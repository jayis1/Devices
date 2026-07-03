/**
 * DriveSync BLE Central — ESP32-S3 NimBLE Stack
 *
 * Connects to Wheel Node, Belt Tag, and OBD-II Dongle.
 * Uses NimBLE (ESP-IDF) for low-power BLE 5.0 central.
 *
 * License: MIT
 */

#include "ble_central.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_tencent.h"
#include <string.h>

static const char *TAG = "drivesync_ble";

/* DriveSync Service UUID (custom) */
static const ble_uuid128_t drivesync_svc_uuid =
    BLE_UUID128_INIT(0x44, 0x52, 0x69, 0x76, 0x65, 0x53, 0x79, 0x6E,
                     0x63, 0x2D, 0x42, 0x4C, 0x45, 0x2D, 0x53, 0x56);

/* Characteristic UUID for data TX/RX */
static const ble_uuid128_t drivesync_char_uuid =
    BLE_UUID128_INIT(0x44, 0x52, 0x69, 0x76, 0x65, 0x53, 0x79, 0x6E,
                     0x63, 0x2D, 0x44, 0x41, 0x54, 0x41, 0x2D, 0x52);

static ble_data_cb_t s_callback = NULL;
static uint16_t s_wheel_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_belt_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_obd_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_wheel_char_handle = 0;
static uint16_t s_belt_char_handle = 0;
static uint16_t s_obd_char_handle = 0;
static uint8_t s_node_count = 0;

/* ── Scan callback ────────────────────────────────────────────────── */

static int scan_callback(struct ble_gap_event *event, void *arg)
{
    if (event->type != BLE_GAP_EVENT_DISC) return 0;

    /* Check if advertising DriveSync service UUID */
    struct ble_hs_adv_fields fields;
    if (ble_hs_adv_parse(event->disc.data, event->disc.length_data, &fields) != 0) {
        return 0;
    }

    /* Connect to discovered DriveSync nodes */
    uint16_t handle = event->disc.handle;
    uint8_t addr[6];
    memcpy(addr, event->disc.addr.val, 6);

    ESP_LOGI(TAG, "Found DriveSync node: %02X:%02X:%02X:%02X:%02X:%02X",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

    /* Connect to it */
    struct ble_gap_conn_params conn_params = {
        .scan_itvl = 16,
        .scan_window = 16,
        .conn_itvl_min = 12,  /* 15 ms */
        .conn_itvl_max = 24,  /* 30 ms */
        .conn_latency = 0,
        .supervision_timeout = 200,  /* 2 sec */
    };

    ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr,
                    3000, &conn_params, NULL, NULL);
    return 0;
}

/* ── Connection event callback ────────────────────────────────────── */

static int gap_event_callback(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Connected to node (handle %d)", event->connect.conn_handle);

            /* Discover services to identify node type */
            /* For stub: assign based on connection order */
            if (s_wheel_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
                s_wheel_conn_handle = event->connect.conn_handle;
            } else if (s_belt_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
                s_belt_conn_handle = event->connect.conn_handle;
            } else if (s_obd_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
                s_obd_conn_handle = event->connect.conn_handle;
            }
            s_node_count++;
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGW(TAG, "Node disconnected (handle %d)", event->disconnect.conn.conn_handle);
        if (event->disconnect.conn.conn_handle == s_wheel_conn_handle) {
            s_wheel_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        } else if (event->disconnect.conn.conn_handle == s_belt_conn_handle) {
            s_belt_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        } else if (event->disconnect.conn.conn_handle == s_obd_conn_handle) {
            s_obd_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        }
        if (s_node_count > 0) s_node_count--;
        break;

    case BLE_GAP_EVENT_NOTIFY_RX: {
        /* Data received from a node */
        const uint8_t *data = event->notify_rx.om_data;
        uint16_t sender_id = (uint16_t)event->notify_rx.conn_handle;
        if (s_callback) {
            s_callback(sender_id, data, event->notify_rx.om_len);
        }
        break;
    }

    default:
        break;
    }
    return 0;
}

/* ── Public API ──────────────────────────────────────────────────── */

void ble_central_init(ble_data_cb_t callback)
{
    s_callback = callback;
    s_node_count = 0;
    ESP_LOGI(TAG, "BLE central initialized");
}

void ble_central_start_scan(void)
{
    uint8_t own_addr_type;
    ble_hs_id_infer_auto(0, &own_addr_type);

    struct ble_gap_disc_params disc_params = {
        .itvl = 32,
        .window = 32,
        .filter_policy = 0,
        .duplicates = 0,
        .active = 1,
    };

    esp_err_t err = ble_gap_disc(own_addr_type, BLE_HS_FOREVER,
                                  &disc_params, gap_event_callback, NULL);
    if (err != 0) {
        ESP_LOGE(TAG, "Scan start failed: %d", err);
    } else {
        ESP_LOGI(TAG, "Scanning for DriveSync nodes...");
    }
}

void ble_central_stop_scan(void)
{
    ble_gap_disc_cancel();
}

static void send_to_handle(uint16_t conn_handle, uint16_t char_handle,
                           const uint8_t *data, uint8_t len)
{
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE || char_handle == 0) return;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) return;

    ble_gattc_notify_custom(conn_handle, char_handle, om);
}

void ble_send_to_wheel(const uint8_t *data, uint8_t len)
{
    send_to_handle(s_wheel_conn_handle, s_wheel_char_handle, data, len);
}

void ble_send_to_belt(const uint8_t *data, uint8_t len)
{
    send_to_handle(s_belt_conn_handle, s_belt_char_handle, data, len);
}

void ble_send_to_obd(const uint8_t *data, uint8_t len)
{
    send_to_handle(s_obd_conn_handle, s_obd_char_handle, data, len);
}

void ble_send_to_all(const uint8_t *data, uint8_t len)
{
    ble_send_to_wheel(data, len);
    ble_send_to_belt(data, len);
    ble_send_to_obd(data, len);
}

uint8_t ble_central_get_node_count(void)
{
    return s_node_count;
}