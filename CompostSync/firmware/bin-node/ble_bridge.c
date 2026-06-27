/*
 * Bin Node — BLE Bridge to Soil Probe
 * firmware/bin-node/ble_bridge.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "csp_protocol.h"
#include "sensor_types.h"
#include <string.h>

static const char *TAG = "BLE_BRIDGE";

extern bin_node_data_t latest_data;

/* BLE service for Soil Probe to connect as central and push data */
#define GATTS_SERVICE_UUID  0xC590
#define GATTS_CHAR_RX       0xC591  /* Soil Probe writes data here */

static uint16_t gatts_if;
static uint16_t conn_id = 0xFFFF;
static uint16_t char_rx_handle;

/* Latest soil probe data (received via BLE) */
static soil_probe_data_t soil_data;

static void gap_handler(esp_gap_ble_cb_event_t event,
                        esp_ble_gap_cb_param_t *param)
{
    /* Handle advertising events */
}

static void gatts_handler(esp_gatts_cb_event_t event,
                           esp_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        gatts_if = param->reg.gatts_if;
        esp_gatt_srvc_id_t svc = {
            .is_primary = true,
            .id = { .uuid = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = GATTS_SERVICE_UUID } } }
        };
        esp_ble_gatts_create_service(gatts_if, &svc, 4);
        break;
    case ESP_GATTS_CREATE_EVT:
        esp_ble_gatts_start_service(param->create.service_handle);
        break;
    case ESP_GATTS_CONNECT_EVT:
        conn_id = param->connect.conn_id;
        ESP_LOGI(TAG, "Soil Probe connected");
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        conn_id = 0xFFFF;
        ESP_LOGI(TAG, "Soil Probe disconnected");
        break;
    case ESP_GATTS_WRITE_EVT:
        if (param->write.handle == char_rx_handle &&
            param->write.len == sizeof(soil_probe_data_t)) {
            memcpy(&soil_data, param->write.value, sizeof(soil_data));
            ESP_LOGI(TAG, "Soil Probe: T1=%.1f T2=%.1f T3=%.1f T4=%.1f pH=%.2f CO2=%d",
                     soil_data.temp_c[0]/10.0f, soil_data.temp_c[1]/10.0f,
                     soil_data.temp_c[2]/10.0f, soil_data.temp_c[3]/10.0f,
                     soil_data.ph/100.0f, soil_data.co2_ppm);
        }
        break;
    default:
        break;
    }
}

void ble_bridge_task(void *pvParameters)
{
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_ble_gap_register_callback(gap_handler);
    esp_ble_gatts_register_callback(gatts_handler);
    esp_ble_gatts_app_register(0);

    esp_ble_gap_set_device_name("CompostSync-Bin");
    esp_ble_adv_data_t adv = {
        .set_scan_rsp = false,
        .include_name = true,
        .service_uuid_len = 2,
        .p_service_uuid = (uint8_t[]){ 0x90, 0xC5 },
        .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
    };
    esp_ble_gap_config_adv_data(&adv);

    esp_ble_adv_params_t adv_p = {
        .adv_int_min = 0x20,
        .adv_int_max = 0x40,
        .adv_type = ADV_TYPE_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
    };
    esp_ble_gap_start_advertising(&adv_p);

    ESP_LOGI(TAG, "BLE bridge started, waiting for Soil Probe...");
    vTaskDelete(NULL);
}