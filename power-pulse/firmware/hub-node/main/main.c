/**
 * PowerPulse Hub Node — Main Entry Point (ESP32-S3, ESP-IDF)
 * 
 * Coordinates all PowerPulse nodes, bridges to cloud via WiFi/MQTT,
 * runs local inference for time-critical alerts, and provides a local
 * web dashboard when internet is unavailable.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"

#include "powerpulse_protocol.h"
#include "hub_config.h"
#include "cc1101.h"
#include "display.h"
#include "cloud_client.h"
#include "ble_mesh.h"

static const char *TAG = "HUB_MAIN";

// ─── Global State ──────────────────────────────────────────────────

typedef struct {
    // WiFi state
    bool wifi_connected;
    bool mqtt_connected;
    bool cloud_reachable;
    
    // Node registry
    uint8_t  num_circuit_monitors;
    uint8_t  num_appliance_tags;
    uint8_t  num_solar_nodes;
    uint16_t registered_nodes[32];  // Addresses of all registered nodes
    
    // Local data cache
    pp_circuit_reading_t circuit_data[PP_CIRCUITS_MAX];
    pp_appliance_data_payload_t appliance_data[32];
    pp_solar_data_payload_t solar_data;
    
    // Alert state
    bool arc_fault_active;
    bool overload_active;
    uint32_t last_alert_time;
    
    // Statistics
    uint32_t packets_rx;
    uint32_t packets_tx;
    uint32_t packets_crc_err;
    uint32_t uptime_seconds;
} hub_state_t;

static hub_state_t g_state = {0};

// ─── Message Queues ────────────────────────────────────────────────

#define SUBGHZ_RX_QUEUE_LEN    32
#define CLOUD_TX_QUEUE_LEN     64
#define ALERT_QUEUE_LEN        16

static QueueHandle_t subghz_rx_queue;
static QueueHandle_t cloud_tx_queue;
static QueueHandle_t alert_queue;

// ─── Sub-GHz Receive Buffer ────────────────────────────────────────

#define SUBGHZ_BUF_SIZE  256
static uint8_t subghz_rx_buf[SUBGHZ_BUF_SIZE];

// ─── LED Indicators ────────────────────────────────────────────────

#define LED_WS2812_PIN   21
#define BUZZER_PIN       35
#define BUTTON_1_PIN     36
#define BUTTON_2_PIN     37

typedef enum {
    LED_STATE_BOOT,
    LED_STATE_WIFI_CONNECTING,
    LED_STATE_WIFI_CONNECTED,
    LED_STATE_NORMAL,
    LED_STATE_ALERT,
    LED_STATE_ERROR,
} led_state_t;

static led_state_t current_led_state = LED_STATE_BOOT;

// ─── Forward Declarations ──────────────────────────────────────────

static void wifi_task(void *pvParameters);
static void subghz_task(void *pvParameters);
static void cloud_task(void *pvParameters);
static void inference_task(void *pvParameters);
static void display_task(void *pvParameters);
static void alert_task(void *pvParameters);
static void heartbeat_task(void *pvParameters);
static void led_task(void *pvParameters);

static void handle_received_frame(const uint8_t *frame, uint16_t len);
static void process_circuit_data(const pp_frame_header_t *header, const uint8_t *payload, uint16_t payload_len);
static void process_arc_fault(const pp_frame_header_t *header, const uint8_t *payload, uint16_t payload_len);
static void process_appliance_data(const pp_frame_header_t *header, const uint8_t *payload, uint16_t payload_len);
static void process_solar_data(const pp_frame_header_t *header, const uint8_t *payload, uint16_t payload_len);
static void process_heartbeat(const pp_frame_header_t *header, const uint8_t *payload, uint16_t payload_len);
static void process_overload_alert(const pp_frame_header_t *header, const uint8_t *payload, uint16_t payload_len);

// ─── Main ──────────────────────────────────────────────────────────

void app_main(void)
{
    ESP_LOGI(TAG, "=== PowerPulse Hub Node Starting ===");
    ESP_LOGI(TAG, "Firmware version: %d.%d", FW_VERSION_MAJOR, FW_VERSION_MINOR);
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create message queues
    subghz_rx_queue = xQueueCreate(SUBGHZ_RX_QUEUE_LEN, SUBGHZ_BUF_SIZE);
    cloud_tx_queue = xQueueCreate(CLOUD_TX_QUEUE_LEN, sizeof(cloud_msg_t));
    alert_queue = xQueueCreate(ALERT_QUEUE_LEN, sizeof(alert_msg_t));
    
    if (!subghz_rx_queue || !cloud_tx_queue || !alert_queue) {
        ESP_LOGE(TAG, "Failed to create message queues!");
        esp_restart();
    }
    
    // Initialize SPI bus (shared by CC1101 and SD card)
    spi_bus_config_t spi_bus = {
        .mosi_io_num = 6,
        .miso_io_num = 5,
        .sclk_io_num = 4,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &spi_bus, SPI_DMA_CH_AUTO));
    
    // Initialize CC1101 Sub-GHz radio
    cc1101_init(SPI2_HOST, 7, 8, 9);  // CS, GDO0, GDO2
    
    // Initialize display (optional)
    display_init();
    
    // Initialize GPIO for buzzer and buttons
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    io_conf.pin_bit_mask = (1ULL << BUTTON_1_PIN) | (1ULL << BUTTON_2_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    
    // Load configuration from NVS
    hub_config_load();
    
    // Create FreeRTOS tasks
    BaseType_t result;
    
    result = xTaskCreatePinnedToCore(wifi_task, "wifi_task", 8192, NULL, 3, NULL, 1);
    if (result != pdPASS) ESP_LOGE(TAG, "Failed to create wifi_task");
    
    result = xTaskCreatePinnedToCore(subghz_task, "subghz_task", 8192, NULL, 4, NULL, 0);
    if (result != pdPASS) ESP_LOGE(TAG, "Failed to create subghz_task");
    
    result = xTaskCreatePinnedToCore(cloud_task, "cloud_task", 12288, NULL, 2, NULL, 1);
    if (result != pdPASS) ESP_LOGE(TAG, "Failed to create cloud_task");
    
    result = xTaskCreatePinnedToCore(inference_task, "inference_task", 16384, NULL, 3, NULL, 0);
    if (result != pdPASS) ESP_LOGE(TAG, "Failed to create inference_task");
    
    result = xTaskCreatePinnedToCore(display_task, "display_task", 4096, NULL, 1, NULL, 1);
    if (result != pdPASS) ESP_LOGE(TAG, "Failed to create display_task");
    
    result = xTaskCreatePinnedToCore(alert_task, "alert_task", 4096, NULL, 5, NULL, 0);
    if (result != pdPASS) ESP_LOGE(TAG, "Failed to create alert_task");
    
    result = xTaskCreatePinnedToCore(heartbeat_task, "heartbeat_task", 4096, NULL, 1, NULL, 0);
    if (result != pdPASS) ESP_LOGE(TAG, "Failed to create heartbeat_task");
    
    result = xTaskCreatePinnedToCore(led_task, "led_task", 2048, NULL, 1, NULL, 1);
    if (result != pdPASS) ESP_LOGE(TAG, "Failed to create led_task");
    
    ESP_LOGI(TAG, "All tasks created. System running.");
    current_led_state = LED_STATE_WIFI_CONNECTING;
}

// ─── Sub-GHz Task ──────────────────────────────────────────────────

static void subghz_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sub-GHz task started on core %d", xPortGetCoreID());
    
    cc1101_set_frequency(868000000);  // 868 MHz
    cc1101_set_data_rate(10);         // 10 kbps
    cc1101_set_tx_power(0x0C);       // +10 dBm
    cc1101_set_rx_mode();
    
    uint16_t seq = 0;
    
    while (1) {
        // Check for received packets
        int16_t rx_len = cc1101_receive_packet(subghz_rx_buf, SUBGHZ_BUF_SIZE, 100);
        
        if (rx_len > 0) {
            g_state.packets_rx++;
            
            // Parse the frame
            pp_frame_header_t header;
            const uint8_t *payload;
            uint16_t payload_len;
            
            int result = pp_frame_parse(subghz_rx_buf, rx_len, &header, &payload, &payload_len);
            
            if (result == 0) {
                ESP_LOGI(TAG, "RX: %s from 0x%04X seq=%u len=%u",
                         pp_msg_type_str(header.type), header.src, header.seq, payload_len);
                handle_received_frame(subghz_rx_buf, rx_len);
                
                // Send ACK back
                uint8_t ack_buf[32];
                pp_ack_payload_t ack = {
                    .acked_seq = header.seq,
                    .status = 0,  // OK
                };
                uint16_t ack_len = pp_frame_build(
                    PP_ADDR_HUB, header.src, PP_MSG_ACK,
                    seq++, (const uint8_t *)&ack, sizeof(ack),
                    ack_buf, sizeof(ack_buf)
                );
                if (ack_len > 0) {
                    cc1101_send_packet(ack_buf, ack_len);
                    cc1101_set_rx_mode();
                    g_state.packets_tx++;
                }
            } else {
                ESP_LOGW(TAG, "Frame parse error: %d (len=%d)", result, rx_len);
                g_state.packets_crc_err++;
            }
        }
        
        // Check for outgoing messages (commands to nodes)
        // ... (handled by other tasks sending via CC1101)
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ─── Handle Received Frame ─────────────────────────────────────────

static void handle_received_frame(const uint8_t *frame, uint16_t len)
{
    pp_frame_header_t header;
    const uint8_t *payload;
    uint16_t payload_len;
    
    if (pp_frame_parse(frame, len, &header, &payload, &payload_len) != 0) {
        return;
    }
    
    switch (header.type) {
        case PP_MSG_CIRCUIT_DATA:
            process_circuit_data(&header, payload, payload_len);
            break;
        case PP_MSG_ARC_FAULT_ALERT:
            process_arc_fault(&header, payload, payload_len);
            break;
        case PP_MSG_OVERLOAD_ALERT:
            process_overload_alert(&header, payload, payload_len);
            break;
        case PP_MSG_APPLIANCE_DATA:
            process_appliance_data(&header, payload, payload_len);
            break;
        case PP_MSG_SOLAR_DATA:
            process_solar_data(&header, payload, payload_len);
            break;
        case PP_MSG_HEARTBEAT:
            process_heartbeat(&header, payload, payload_len);
            break;
        case PP_MSG_ACK:
            // ACK received, update state
            ESP_LOGI(TAG, "ACK received for seq %u", ((pp_ack_payload_t *)payload)->acked_seq);
            break;
        default:
            ESP_LOGW(TAG, "Unhandled message type: 0x%02X", header.type);
            break;
    }
}

// ─── Process Circuit Data ──────────────────────────────────────────

static void process_circuit_data(const pp_frame_header_t *header, const uint8_t *payload, uint16_t payload_len)
{
    if (payload_len < sizeof(pp_circuit_data_header_t)) {
        ESP_LOGW(TAG, "Circuit data payload too short: %u", payload_len);
        return;
    }
    
    const pp_circuit_data_header_t *cdh = (const pp_circuit_data_header_t *)payload;
    const pp_circuit_reading_t *readings = (const pp_circuit_reading_t *)(payload + sizeof(pp_circuit_data_header_t));
    
    uint16_t voltage_mv = cdh->voltage_mv;
    float voltage = voltage_mv / 1000.0f;
    
    ESP_LOGI(TAG, "Circuit data: V=%.1f Hz=%.2f active=%d mask=0x%02X",
             voltage, cdh->frequency_cph / 100.0f, cdh->num_active, cdh->circuit_mask);
    
    for (int i = 0; i < cdh->num_active && i < PP_CIRCUITS_MAX; i++) {
        g_state.circuit_data[readings[i].circuit_id] = readings[i];
        ESP_LOGI(TAG, "  Circuit %d: I=%.1fA P=%uW PF=%.2f E=%uWh",
                 readings[i].circuit_id,
                 readings[i].current_ma / 1000.0f,
                 readings[i].power_w,
                 readings[i].power_factor / 10000.0f,
                 readings[i].energy_wh);
    }
    
    // Queue for cloud upload
    cloud_msg_t msg = {
        .type = PP_MSG_CIRCUIT_DATA,
        .source = header->src,
        .timestamp = esp_timer_get_time() / 1000,  // ms
    };
    memcpy(msg.payload, payload, min(payload_len, sizeof(msg.payload)));
    msg.payload_len = payload_len;
    xQueueSend(cloud_tx_queue, &msg, pdMS_TO_TICKS(100));
    
    // Also queue for local inference (anomaly detection)
    // The inference task will pull from circuit_data directly
}

// ─── Process Arc Fault Alert ───────────────────────────────────────

static void process_arc_fault(const pp_frame_header_t *header, const uint8_t *payload, uint16_t payload_len)
{
    if (payload_len < sizeof(pp_arc_fault_payload_t)) {
        return;
    }
    
    const pp_arc_fault_payload_t *arc = (const pp_arc_fault_payload_t *)payload;
    
    ESP_LOGE(TAG, "⚠️ ARC FAULT on circuit %d! Confidence=%d%% Type=%d Severity=%d Duration=%ums",
             arc->circuit_id, arc->confidence_pct, arc->arc_type,
             arc->severity, arc->duration_ms);
    
    g_state.arc_fault_active = true;
    g_state.last_alert_time = esp_timer_get_time() / 1000;
    
    // Immediate alert
    alert_msg_t alert = {
        .type = ALERT_ARC_FAULT,
        .severity = arc->severity,
        .circuit_id = arc->circuit_id,
        .confidence = arc->confidence_pct,
        .timestamp = arc->timestamp_unix,
    };
    xQueueSend(alert_queue, &alert, 0);  // Don't block on alert queue
    
    // Forward to cloud immediately (high priority)
    cloud_msg_t msg = {
        .type = PP_MSG_ARC_FAULT_ALERT,
        .source = header->src,
        .timestamp = esp_timer_get_time() / 1000,
        .priority = CLOUD_PRIORITY_HIGH,
    };
    memcpy(msg.payload, payload, min(payload_len, sizeof(msg.payload)));
    msg.payload_len = payload_len;
    xQueueSendToFront(cloud_tx_queue, &msg, pdMS_TO_TICKS(1000));
}

// ─── Process Appliance Data ────────────────────────────────────────

static void process_appliance_data(const pp_frame_header_t *header, const uint8_t *payload, uint16_t payload_len)
{
    if (payload_len < sizeof(pp_appliance_data_payload_t)) {
        return;
    }
    
    const pp_appliance_data_payload_t *ap = (const pp_appliance_data_payload_t *)payload;
    
    // Store in cache
    if (ap->tag_id < 32) {
        g_state.appliance_data[ap->tag_id] = *ap;
    }
    
    ESP_LOGI(TAG, "Appliance %d: V=%.1f I=%.1fA P=%uW PF=%.2f E=%luWh Relay=%d Temp=%d°C",
             ap->tag_id,
             ap->voltage_mv / 1000.0f,
             ap->current_ma / 1000.0f,
             ap->power_w,
             ap->power_factor / 10000.0f,
             (unsigned long)ap->energy_wh,
             ap->relay_state,
             ap->temperature_c);
    
    // Queue for cloud
    cloud_msg_t msg = {
        .type = PP_MSG_APPLIANCE_DATA,
        .source = header->src,
        .timestamp = esp_timer_get_time() / 1000,
        .priority = CLOUD_PRIORITY_NORMAL,
    };
    memcpy(msg.payload, payload, min(payload_len, sizeof(msg.payload)));
    msg.payload_len = payload_len;
    xQueueSend(cloud_tx_queue, &msg, pdMS_TO_TICKS(100));
}

// ─── Process Solar Data ────────────────────────────────────────────

static void process_solar_data(const pp_frame_header_t *header, const uint8_t *payload, uint16_t payload_len)
{
    if (payload_len < sizeof(pp_solar_data_payload_t)) {
        return;
    }
    
    const pp_solar_data_payload_t *sd = (const pp_solar_data_payload_t *)payload;
    g_state.solar_data = *sd;
    
    ESP_LOGI(TAG, "Solar: PV=%.1fV/%.1fA/%uW Batt=%.1fV SoC=%d%% Load=%uW Duty=%d%% Temp=%d°C",
             sd->pv_voltage_mv / 1000.0f,
             sd->pv_current_ma / 1000.0f,
             sd->pv_power_w,
             sd->batt_voltage_mv / 1000.0f,
             sd->soc_pct,
             sd->load_power_w,
             sd->mppt_duty_pct,
             sd->heatsink_temp_c);
    
    // Queue for cloud
    cloud_msg_t msg = {
        .type = PP_MSG_SOLAR_DATA,
        .source = header->src,
        .timestamp = esp_timer_get_time() / 1000,
        .priority = CLOUD_PRIORITY_NORMAL,
    };
    memcpy(msg.payload, payload, min(payload_len, sizeof(msg.payload)));
    msg.payload_len = payload_len;
    xQueueSend(cloud_tx_queue, &msg, pdMS_TO_TICKS(100));
}

// ─── Process Overload Alert ────────────────────────────────────────

static void process_overload_alert(const pp_frame_header_t *header, const uint8_t *payload, uint16_t payload_len)
{
    if (payload_len < sizeof(pp_overload_payload_t)) {
        return;
    }
    
    const pp_overload_payload_t *ol = (const pp_overload_payload_t *)payload;
    
    ESP_LOGW(TAG, "⚠️ OVERLOAD on circuit %d: I=%.1fA (threshold=%.1fA, %d%%)",
             ol->circuit_id,
             ol->current_ma / 1000.0f,
             ol->threshold_ma / 1000.0f,
             ol->overload_pct);
    
    g_state.overload_active = true;
    
    // Alert
    alert_msg_t alert = {
        .type = ALERT_OVERLOAD,
        .severity = 2,
        .circuit_id = ol->circuit_id,
        .timestamp = ol->timestamp_unix,
    };
    xQueueSend(alert_queue, &alert, 0);
}

// ─── Process Heartbeat ─────────────────────────────────────────────

static void process_heartbeat(const pp_frame_header_t *header, const uint8_t *payload, uint16_t payload_len)
{
    if (payload_len < sizeof(pp_heartbeat_payload_t)) {
        return;
    }
    
    const pp_heartbeat_payload_t *hb = (const pp_heartbeat_payload_t *)payload;
    
    ESP_LOGI(TAG, "Heartbeat from 0x%04X: type=%s batt=%d%% uptime=%umin circuits=%d RSSI=%ddBm",
             header->src, pp_node_type_str(hb->node_type),
             hb->battery_pct, hb->uptime_min, hb->num_circuits, hb->signal_rssi);
    
    // Register node if not already known
    bool found = false;
    for (int i = 0; i < 32; i++) {
        if (g_state.registered_nodes[i] == header->src) {
            found = true;
            break;
        }
    }
    if (!found) {
        for (int i = 0; i < 32; i++) {
            if (g_state.registered_nodes[i] == 0) {
                g_state.registered_nodes[i] = header->src;
                switch (hb->node_type) {
                    case PP_NODE_CIRCUIT_MONITOR: g_state.num_circuit_monitors++; break;
                    case PP_NODE_APPLIANCE_TAG:  g_state.num_appliance_tags++; break;
                    case PP_NODE_SOLAR:          g_state.num_solar_nodes++; break;
                    default: break;
                }
                break;
            }
        }
    }
}

// ─── WiFi Task ──────────────────────────────────────────────────────

static void wifi_task(void *pvParameters)
{
    ESP_LOGI(TAG, "WiFi task started");
    
    // Initialize WiFi in STA mode
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    
    // Load WiFi credentials from NVS
    char ssid[64] = {0};
    char password[64] = {0};
    hub_config_get_wifi(ssid, sizeof(ssid), password, sizeof(password));
    
    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
    
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            wifi_event_group(),
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, pdFALSE, portMAX_DELAY
        );
        
        if (bits & WIFI_CONNECTED_BIT) {
            g_state.wifi_connected = true;
            current_led_state = LED_STATE_WIFI_CONNECTED;
        } else {
            g_state.wifi_connected = false;
            current_led_state = LED_STATE_WIFI_CONNECTING;
            esp_wifi_connect();
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ─── Inference Task (Local ML) ──────────────────────────────────────

static void inference_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Inference task started on core %d", xPortGetCoreID());
    
    // Load TensorFlow Lite Micro model for anomaly detection
    // (In production, this would load the model from flash)
    ESP_LOGI(TAG, "Loading local ML model...");
    vTaskDelay(pdMS_TO_TICKS(1000));  // Simulated model load time
    
    float anomaly_threshold = 0.85f;  // Reconstruction error threshold
    
    while (1) {
        // Run anomaly detection on cached circuit data
        // This would use the TFLite Micro interpreter in production
        for (int i = 0; i < PP_CIRCUITS_MAX; i++) {
            if (g_state.circuit_data[i].current_ma > 0) {
                // Simplified anomaly score calculation
                // In production: feed 60-second window to LSTM autoencoder
                float current_a = g_state.circuit_data[i].current_ma / 1000.0f;
                float power_w = g_state.circuit_data[i].power_w;
                float pf = abs(g_state.circuit_data[i].power_factor) / 10000.0f;
                
                // Check for suspicious patterns:
                // 1. Power factor suddenly dropped (possible motor failure)
                // 2. Current spike without corresponding power increase
                // 3. Very low power factor on high-current circuit (phantom load)
                
                if (pf < 0.5f && current_a > 2.0f) {
                    ESP_LOGW(TAG, "Low power factor on circuit %d: PF=%.2f I=%.1fA — possible phantom load",
                             i, pf, current_a);
                }
                
                // Overload check (should be on circuit monitor, but double-check)
                if (current_a > 20.0f) {  // 20A threshold
                    ESP_LOGW(TAG, "High current on circuit %d: %.1fA", i, current_a);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));  // Run every 5 seconds
    }
}

// ─── Display Task ───────────────────────────────────────────────────

static void display_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Display task started");
    
    display_clear();
    display_draw_text(0, 0, "PowerPulse Hub", FONT_LARGE, COLOR_WHITE);
    display_draw_text(0, 20, "Starting...", FONT_SMALL, COLOR_CYAN);
    display_update();
    
    while (1) {
        display_clear();
        
        // Draw header
        display_draw_text(0, 0, "PowerPulse Hub", FONT_LARGE, COLOR_WHITE);
        
        // Draw status
        char buf[32];
        snprintf(buf, sizeof(buf), "Nodes: %d CM + %d AT + %d SN",
                 g_state.num_circuit_monitors,
                 g_state.num_appliance_tags,
                 g_state.num_solar_nodes);
        display_draw_text(0, 20, buf, FONT_SMALL, COLOR_CYAN);
        
        snprintf(buf, sizeof(buf), "WiFi: %s MQTT: %s",
                 g_state.wifi_connected ? "OK" : "--",
                 g_state.mqtt_connected ? "OK" : "--");
        display_draw_text(0, 32, buf, FONT_SMALL,
                          g_state.wifi_connected ? COLOR_GREEN : COLOR_RED);
        
        // Draw total power
        uint32_t total_watts = 0;
        for (int i = 0; i < PP_CIRCUITS_MAX; i++) {
            total_watts += g_state.circuit_data[i].power_w;
        }
        snprintf(buf, sizeof(buf), "Total: %luW", (unsigned long)total_watts);
        display_draw_text(0, 48, buf, FONT_LARGE, COLOR_YELLOW);
        
        // Draw solar if available
        if (g_state.num_solar_nodes > 0) {
            snprintf(buf, sizeof(buf), "Solar: %uW  Batt: %d%%",
                     g_state.solar_data.pv_power_w, g_state.solar_data.soc_pct);
            display_draw_text(0, 68, buf, FONT_SMALL, COLOR_GREEN);
        }
        
        // Draw alerts
        if (g_state.arc_fault_active) {
            display_draw_text(0, 84, "!! ARC FAULT !!", FONT_LARGE, COLOR_RED);
        } else if (g_state.overload_active) {
            display_draw_text(0, 84, "! OVERLOAD !", FONT_LARGE, COLOR_RED);
        }
        
        // Draw packet stats
        snprintf(buf, sizeof(buf), "RX:%lu TX:%lu ERR:%lu",
                 (unsigned long)g_state.packets_rx,
                 (unsigned long)g_state.packets_tx,
                 (unsigned long)g_state.packets_crc_err);
        display_draw_text(0, 104, buf, FONT_SMALL, COLOR_GRAY);
        
        display_update();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ─── Alert Task ─────────────────────────────────────────────────────

static void alert_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Alert task started");
    
    alert_msg_t alert;
    
    while (1) {
        if (xQueueReceive(alert_queue, &alert, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // Sound buzzer pattern based on severity
            int beeps = alert.severity;
            for (int i = 0; i < beeps; i++) {
                gpio_set_level(BUZZER_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(BUZZER_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            
            // Set LED state to alert
            current_led_state = LED_STATE_ALERT;
            
            ESP_LOGE(TAG, "ALERT: type=%d severity=%d circuit=%d",
                     alert.type, alert.severity, alert.circuit_id);
            
            // Return to normal after 30 seconds
            vTaskDelay(pdMS_TO_TICKS(30000));
            if (g_state.arc_fault_active) {
                g_state.arc_fault_active = false;
            }
            if (g_state.overload_active) {
                g_state.overload_active = false;
            }
            current_led_state = LED_STATE_NORMAL;
        }
    }
}

// ─── Heartbeat Task ─────────────────────────────────────────────────

static void heartbeat_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Heartbeat task started");
    
    uint16_t seq = 0;
    
    while (1) {
        // Broadcast heartbeat to all nodes
        pp_heartbeat_payload_t hb = {
            .node_type = PP_NODE_HUB,
            .battery_pct = 255,  // Mains powered
            .uptime_min = (uint16_t)(g_state.uptime_seconds / 60),
            .num_circuits = g_state.num_circuit_monitors + g_state.num_appliance_tags + g_state.num_solar_nodes,
            .firmware_ver = (FW_VERSION_MAJOR << 4) | FW_VERSION_MINOR,
            .signal_rssi = -50,  // Approximate (hub is WiFi)
            .flags = 0x06,  // bit1=calibrated, bit2=sd_card
        };
        
        uint8_t tx_buf[64];
        uint16_t tx_len = pp_frame_build(
            PP_ADDR_HUB, PP_ADDR_BROADCAST, PP_MSG_HEARTBEAT,
            seq++, (const uint8_t *)&hb, sizeof(hb),
            tx_buf, sizeof(tx_buf)
        );
        
        if (tx_len > 0) {
            cc1101_send_packet(tx_buf, tx_len);
            cc1101_set_rx_mode();
            g_state.packets_tx++;
        }
        
        // Update uptime
        g_state.uptime_seconds += 60;
        
        vTaskDelay(pdMS_TO_TICKS(60000));  // Every 60 seconds
    }
}

// ─── LED Task ───────────────────────────────────────────────────────

static void led_task(void *pvParameters)
{
    // Simplified LED state machine using WS2812B
    // In production: use RMT peripheral for WS2812B protocol
    
    while (1) {
        switch (current_led_state) {
            case LED_STATE_BOOT:
                // Solid blue
                break;
            case LED_STATE_WIFI_CONNECTING:
                // Blinking yellow
                break;
            case LED_STATE_WIFI_CONNECTED:
                // Solid green for 2 seconds, then normal
                if (g_state.cloud_reachable) {
                    current_led_state = LED_STATE_NORMAL;
                }
                break;
            case LED_STATE_NORMAL:
                // Slow breathing green
                break;
            case LED_STATE_ALERT:
                // Fast blinking red
                break;
            case LED_STATE_ERROR:
                // Solid red
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}