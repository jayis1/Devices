/*
 * Hub — BLE GATT Server for mobile app communication
 * firmware/hub/ble_service.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "csp_protocol.h"

static const char *TAG = "BLE_SVC";

/* GATT service UUID: 0000C580-1212-EFDE-1523-785FEABCD123 */
#define GATTS_SERVICE_UUID   0xC580
#define GATTS_CHAR_READ      0xC581
#define GATTS_CHAR_WRITE     0xC582
#define GATTS_CHAR_NOTIFY    0xC583

static uint16_t gatts_if;
static uint16_t conn_id = 0xFFFF;
static uint16_t service_handle;
static uint16_t char_read_handle, char_write_handle, char_notify_handle;
static bool notify_enabled = false;

/* Latest sensor snapshot for BLE read */
static char snapshot_json[512];

/* Called from other tasks to update snapshot */
void ble_update_snapshot(const char *json)
{
    strncpy(snapshot_json, json, sizeof(snapshot_json) - 1);
    snapshot_json[sizeof(snapshot_json) - 1] = '\0';

    if (notify_enabled && conn_id != 0xFFFF) {
        esp_ble_gatts_send_indicate(gatts_if, conn_id, char_notify_handle,
                                    strlen(snapshot_json), (uint8_t *)snapshot_json, false);
    }
}

static void gap_event_handler(esp_gap_ble_cb_event_t event,
                               esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Adv data set");
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        ESP_LOGI(TAG, "Adv started, ret=%d", param->adv_start_cmpl.status);
        break;
    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event,
                                  esp_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "GATTS registered, status=%d", param->reg.status);
        gatts_if = param->reg.gatts_if;

        /* Create service */
        esp_gatt_srvc_id_t svc_id = {
            .is_primary = true,
            .id = {
                .uuid = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = GATTS_SERVICE_UUID } }
            }
        };
        esp_ble_gatts_create_service(gatts_if, &svc_id, 6);
        break;

    case ESP_GATTS_CREATE_EVT:
        service_handle = param->create.service_handle;
        esp_ble_gatts_start_service(service_handle);
        ESP_LOGI(TAG, "Service created handle=%d", service_handle);
        break;

    case ESP_GATTS_CONNECT_EVT:
        conn_id = param->connect.conn_id;
        ESP_LOGI(TAG, "Client connected, conn_id=%d", conn_id);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        conn_id = 0xFFFF;
        notify_enabled = false;
        ESP_LOGI(TAG, "Client disconnected");
        break;

    case ESP_GATTS_READ_EVT:
        /* Return current snapshot */
        esp_gatt_rsp_t rsp = {0};
        rsp.attr_value.len = strlen(snapshot_json);
        memcpy(rsp.attr_value.value, snapshot_json, rsp.attr_value.len);
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
                                     param->read.trans_id, ESP_GATT_OK, &rsp);
        break;

    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "Write event, len=%d", param->write.len);
        /* Parse command JSON here and act on it */
        break;

    case ESP_GATTS_WRITE_NVAR_EVT:
        if (param->write.handle == char_notify_handle) {
            notify_enabled = (param->write.value[0] & 0x01) != 0;
        }
        break;

    default:
        break;
    }
}

void ble_service_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting BLE service...");

    /* Init BT controller */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gatts_app_register(0);

    /* Set advertising data */
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,
        .min_interval = 0x20,
        .max_interval = 0x40,
        .appearance = 0x00,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = 2,
        .p_service_uuid = (uint8_t[]){ 0x80, 0xC5 }, /* 0xC580 */
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };
    esp_ble_gap_config_adv_data(&adv_data);

    /* Set device name */
    esp_ble_gap_set_device_name("CompostSync Hub");

    /* Start advertising */
    esp_ble_adv_params_t adv_params = {
        .adv_int_min = 0x20,
        .adv_int_max = 0x40,
        .adv_type = ADV_TYPE_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
    };
    esp_ble_gap_start_advertising(&adv_params);

    ESP_LOGI(TAG, "BLE advertising started");

    /* Task stays alive — events are handled via callbacks */
    vTaskDelete(NULL);
}