/**
 * JointSync Hub — BLE Central Manager
 *
 * Manages BLE connections to Joint Tags and Joint Scanner.
 * ESP32-S3 BLE 5.0 central role.
 *
 * License: MIT
 */

#include "ble_central.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble.h"
#include "esp_gattc_api.h"

static const char *TAG = "ble_central";

#define JS_BLE_SERVICE_UUID  0x4A53   /* "JS" */
#define JS_BLE_CHAR_DATA     0x01
#define JS_BLE_CHAR_CMD      0x02

static ble_data_cb_t g_callback = NULL;
static bool g_scanning = false;

/* Connection state for up to 8 peripherals */
typedef struct {
    uint16_t    conn_id;
    uint16_t    sender_id;
    bool        connected;
    uint16_t    char_data_handle;
    uint16_t    char_cmd_handle;
} ble_conn_t;

static ble_conn_t g_conns[8];
static uint8_t g_conn_count = 0;

void ble_central_init(ble_data_cb_t callback)
{
    g_callback = callback;
    memset(g_conns, 0, sizeof(g_conns));

    /* Initialize BLE controller + Bluedroid stack */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_LOGI(TAG, "BLE central initialized");
}

void ble_central_start_scan(void)
{
    esp_ble_gap_config_scan(&scan_params);
    esp_ble_gap_start_scanning(0);  /* Continuous */
    g_scanning = true;
    ESP_LOGI(TAG, "BLE scanning started");
}

void ble_central_stop_scan(void)
{
    esp_ble_gap_stop_scanning();
    g_scanning = false;
    ESP_LOGI(TAG, "BLE scanning stopped");
}

void ble_send_to_all(uint8_t *data, uint8_t len)
{
    for (int i = 0; i < g_conn_count; i++) {
        if (g_conns[i].connected && g_conns[i].char_cmd_handle != 0) {
            esp_ble_gattc_write_char(esp_bluedroid_get_status(),
                                     g_conns[i].conn_id,
                                     g_conns[i].char_cmd_handle,
                                     len, data, ESP_GATT_WRITE_TYPE_RSP);
        }
    }
}

void ble_send_to_scanner(uint8_t *data, uint8_t len)
{
    /* Find scanner connection (sender_id >= 0x0200) */
    for (int i = 0; i < g_conn_count; i++) {
        if (g_conns[i].connected &&
            g_conns[i].sender_id >= JS_SCANNER_ID_BASE &&
            g_conns[i].char_cmd_handle != 0) {
            esp_ble_gattc_write_char(esp_bluedroid_get_status(),
                                     g_conns[i].conn_id,
                                     g_conns[i].char_cmd_handle,
                                     len, data, ESP_GATT_WRITE_TYPE_RSP);
            return;
        }
    }
    ESP_LOGW(TAG, "Scanner not connected");
}

void ble_send_to_node(uint16_t sender_id, uint8_t *data, uint8_t len)
{
    for (int i = 0; i < g_conn_count; i++) {
        if (g_conns[i].connected && g_conns[i].sender_id == sender_id &&
            g_conns[i].char_cmd_handle != 0) {
            esp_ble_gattc_write_char(esp_bluedroid_get_status(),
                                     g_conns[i].conn_id,
                                     g_conns[i].char_cmd_handle,
                                     len, data, ESP_GATT_WRITE_TYPE_RSP);
            return;
        }
    }
    ESP_LOGW(TAG, "Node 0x%04X not connected", sender_id);
}

uint8_t ble_get_connected_count(void)
{
    uint8_t count = 0;
    for (int i = 0; i < g_conn_count; i++) {
        if (g_conns[i].connected) count++;
    }
    return count;
}