/**
 * SightSync Vision Hub — BLE Central Implementation
 *
 * Scans for SightSync Eye Tag (nRF52840) and connects via BLE 5.0.
 * Uses NimBLE stack on ESP32-S3.
 *
 * License: MIT
 */

#include "ble_central.h"
#include "../common/ble_periph.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"

static const char *TAG = "ble_central";
static ble_central_rx_cb_t s_rx_cb = NULL;
static bool s_connected = false;
static uint16_t s_conn_handle = 0;

/* UUIDs for SightSync service and characteristics */
static ble_uuid128_t ss_svc_uuid = BLE_UUID128_INIT(
    0x9f, 0x8e, 0x7d, 0x6c, 0x3a, 0x2b, 0x01, 0xb5,
    0x8e, 0x4c, 0x30, 0x1c, 0x01, 0x00, 0x5f, 0x8a);

/* ── Scan callback ──────────────────────────────────────────────── */

static int on_scan_result(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    if (event->type == BLE_GAP_EVENT_DISC) {
        /* Check for SightSync service in advertisement */
        struct ble_hs_adv_fields fields;
        if (ble_hs_adv_parse_fields(&fields, event->disc.adv_data,
                                     event->disc.adv_data_len) == 0) {
            if (fields.name != NULL && strstr((char *)fields.name, "SightSync") != NULL) {
                ESP_LOGI(TAG, "Found SightSync Eye Tag — connecting...");
                ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr,
                                30000, NULL, on_gap_event, NULL);
            }
        }
    }
    return 0;
}

/* ── GAP event callback ───────────────────────────────────────────── */

static int on_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            s_connected = true;
            ESP_LOGI(TAG, "Connected to Eye Tag (handle=%d)", s_conn_handle);
            /* Discover services and subscribe to TX characteristic notifications */
            /* TODO: ble_gattc_disc_all_svcs + disc_all_chrs */
        } else {
            ESP_LOGW(TAG, "Connection failed, status=%d", event->connect.status);
            s_connected = false;
            ble_central_start_scan();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected from Eye Tag");
        s_connected = false;
        ble_central_start_scan();
        break;

    case BLE_GAP_EVENT_NOTIFY_RX: {
        /* Notification received from Eye Tag */
        const uint8_t *data = event->notify_rx.om->om_data;
        uint16_t len = OS_MBUF_PKTLEN(event->notify_rx.om);

        sightsync_header_t hdr;
        const uint8_t *payload = NULL;
        if (sightsync_decode(data, (uint8_t)len, &hdr, &payload)) {
            if (s_rx_cb != NULL) {
                s_rx_cb(&hdr, payload);
            }
        }
        break;
    }

    default:
        break;
    }
    return 0;
}

/* ── Public API ──────────────────────────────────────────────────── */

void ble_central_init(ble_central_rx_cb_t rx_cb)
{
    s_rx_cb = rx_cb;
    /* TODO: initialize NimBLE stack, configure scan parameters */
    ESP_LOGI(TAG, "BLE Central initialized");
}

void ble_central_start_scan(void)
{
    struct ble_gap_disc_params disc = {
        .itvl = 32,
        .window = 16,
        .filter_policy = BLE_HCI_SCAN_FILT_NO_WL,
        .passive = 0,
        .limited = 0,
    };
    /* ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 0, &disc, NULL, on_gap_event, NULL); */
    ESP_LOGI(TAG, "BLE scan started for Eye Tag");
}

void ble_central_stop_scan(void)
{
    /* ble_gap_disc_cancel(); */
}

bool ble_central_is_connected(void)
{
    return s_connected;
}

void ble_central_send(const uint8_t *data, uint8_t len)
{
    if (s_connected) {
        /* TODO: write to RX characteristic of Eye Tag */
        (void)data; (void)len;
    }
}