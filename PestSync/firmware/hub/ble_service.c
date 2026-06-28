/*
 * Hub — BLE GATT Server (Mobile App bridge)
 * firmware/hub/ble_service.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/util/util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "psp_protocol.h"
#include "sensor_types.h"

static const char *TAG = "BLE";

/* PestSync BLE Service UUID */
static const ble_uuid128_t svc_uuid =
    BLE_UUID128_INIT(0x23, 0xD1, 0xAB, 0xFE, 0x5F, 0x78, 0x23, 0x15,
                     0xDE, 0xEF, 0x12, 0x12, 0x50, 0xE5, 0x00, 0x00);
static const ble_uuid128_t read_uuid =
    BLE_UUID128_INIT(0x24, 0xD1, 0xAB, 0xFE, 0x5F, 0x78, 0x23, 0x15,
                     0xDE, 0xEF, 0x12, 0x12, 0x50, 0xE5, 0x00, 0x00);
static const ble_uuid128_t write_uuid =
    BLE_UUID128_INIT(0x25, 0xD1, 0xAB, 0xFE, 0x5F, 0x78, 0x23, 0x15,
                     0xDE, 0xEF, 0x12, 0x12, 0x50, 0xE5, 0x00, 0x00);
static const ble_uuid128_t notify_uuid =
    BLE_UUID128_INIT(0x26, 0xD1, 0xAB, 0xFE, 0x5F, 0x78, 0x23, 0x15,
                     0xDE, 0xEF, 0x12, 0x12, 0x50, 0xE5, 0x00, 0x00);

static uint16_t conn_handle_global = BLE_HS_CONN_HANDLE_NONE;
static uint16_t notify_val_handle;
static bool notify_enabled = false;

/* Build JSON snapshot for mobile app */
static int build_snapshot(char *buf, size_t maxlen)
{
    int len = snprintf(buf, maxlen,
        "{\"sentinel\":{\"pest\":\"%s\",\"conf\":%d,\"count\":%d,\"thermal\":%.1f,\"bat\":%d},"
        "\"trap\":{\"status\":\"%s\",\"weight\":%d,\"bait\":%d,\"bat\":%d},"
        "\"deterrent\":{\"us\":%d,\"strobe\":%d,\"oil\":%d,\"bat\":%d}}",
        pest_class_name(g_sentinel_data.pest_class),
        g_sentinel_data.confidence,
        g_sentinel_data.count_since_last,
        g_sentinel_data.thermal_max_c / 10.0f,
        g_sentinel_data.battery_pct,
        trap_status_name(g_trap_data.trap_status),
        g_trap_data.catch_weight_g,
        g_trap_data.bait_level,
        g_trap_data.battery_pct,
        g_deterrent_data.ultrasonic_active,
        g_deterrent_data.strobe_active,
        g_deterrent_data.oil_level,
        g_deterrent_data.battery_pct
    );
    return len;
}

static int gatt_read_cb(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    char snapshot[512];
    int len = build_snapshot(snapshot, sizeof(snapshot));
    os_mbuf_append(ctxt->om, snapshot, len);
    return 0;
}

static int gatt_write_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    char buf[256];
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len > sizeof(buf) - 1) len = sizeof(buf) - 1;
    os_mbuf_copydata(ctxt->om, 0, len, buf);
    buf[len] = '\0';
    ESP_LOGI(TAG, "Command received: %s", buf);
    /* Parse JSON command and dispatch (e.g., trigger camera, set deterrent) */
    return 0;
}

static int gatt_notify_subscribe_cb(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
        uint16_t val;
        os_mbuf_copydata(ctxt->om, 0, sizeof(val), &val);
        notify_enabled = (val == 0x0001);
        ESP_LOGI(TAG, "Notifications %s", notify_enabled ? "enabled" : "disabled");
    }
    return 0;
}

static const struct ble_gatt_svc_def svc_defs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = (ble_uuid_t *)&svc_uuid,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = (ble_uuid_t *)&read_uuid,
                .access_cb = gatt_read_cb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = (ble_uuid_t *)&write_uuid,
                .access_cb = gatt_write_cb,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {
                .uuid = (ble_uuid_t *)&notify_uuid,
                .access_cb = gatt_notify_subscribe_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
                .val_handle = &notify_val_handle,
            },
            { 0 }
        },
    },
    { 0 }
};

static void on_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        conn_handle_global = event->connect.conn_handle;
        ESP_LOGI(TAG, "Mobile app connected");
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        conn_handle_global = BLE_HS_CONN_HANDLE_NONE;
        notify_enabled = false;
        ESP_LOGI(TAG, "Mobile app disconnected");
        break;
    default:
        break;
    }
}

static void ble_app_sync(void)
{
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(svc_defs);
    ble_gatts_add_svcs(svc_defs);
}

static void ble_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "BLE advertising PestSync Hub...");
        ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER,
                          NULL, on_gap_event, NULL);
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void ble_service_task(void *pvParameters)
{
    ESP_LOGI(TAG, "BLE service starting...");
    nimble_port_init();
    ble_app_sync();
    nimble_port_freertos_init(ble_task);
}