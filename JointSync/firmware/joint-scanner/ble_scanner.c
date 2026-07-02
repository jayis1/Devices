/**
 * JointSync Joint Scanner — BLE Peripheral
 *
 * ESP32-S3 BLE 5.0 peripheral for communication with Hub.
 *
 * License: MIT
 */

#include "ble_scanner.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble.h"
#include "esp_gatts_api.h"

static const char *TAG = "ble_scanner";

#define JOINTSYNC_SERVICE_UUID  0x4A53
#define JOINTSYNC_CHAR_DATA     0x4A01
#define JOINTSYNC_CHAR_CMD      0x4A02

static ble_scanner_cb_t g_cmd_cb = NULL;
static uint16_t g_conn_handle = 0xFFFF;
static uint16_t g_char_data_handle = 0;
static uint16_t g_char_cmd_handle = 0;

void ble_scanner_init(ble_scanner_cb_t cmd_cb)
{
    g_cmd_cb = cmd_cb;

    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    /* GATT server setup would go here — simplified for stub */
    ESP_LOGI(TAG, "BLE scanner peripheral initialized");
}

void ble_scanner_advertise(void)
{
    /* Start advertising */
    esp_ble_gap_config_adv_data_raw((uint8_t *)"JointSync_Scanner", 17);
    esp_ble_gap_start_advertising(&adv_params);
    ESP_LOGI(TAG, "BLE advertising started");
}

void ble_scanner_send(uint8_t *data, uint8_t len)
{
    if (g_conn_handle == 0xFFFF) return;

    /* Send via notification on data characteristic */
    esp_ble_gatts_send_indicate(g_conn_handle, g_char_data_handle, len, data, false);
}

bool ble_scanner_is_connected(void)
{
    return g_conn_handle != 0xFFFF;
}