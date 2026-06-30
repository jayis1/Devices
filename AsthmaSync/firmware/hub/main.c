/**
 * AsthmaSync — Hub Main Firmware
 * ==============================
 * ESP32-S3-WROOM-1-N16R8 running ESP-IDF v5.1 + FreeRTOS.
 *
 * The Hub coordinates:
 *   1. Sub-GHz TDMA mesh (coordinator for Air Sentinel)
 *   2. BLE 5.0 central (Inhaler Tag + Wheeze Band)
 *   3. WiFi → MQTT/TLS cloud connection
 *   4. Edge ML inference (tflite-micro)
 *   5. Status display (ILI9341 TFT)
 *   6. Audible + visual alerts (buzzer + RGB LED)
 *
 * License: MIT
 */

#include "config.h"
#include "edge_ml.h"
#include "mqtt.h"
#include "../common/protocol.h"
#include "../common/radio.h"

/* ── ESP-IDF includes ──────────────────────────────────── */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_psram.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

static const char *TAG = "asthmasync.hub";

/* ── Global State ──────────────────────────────────────── */
static alert_zone_t s_current_zone = ZONE_GREEN;
static uint8_t s_rescue_count_week = 0;
static uint16_t s_node_count = 0;
static QueueHandle_t s_event_queue = NULL;
static SemaphoreHandle_t s_zone_mutex = NULL;

/* ── Forward Declarations ──────────────────────────────── */
static void task_mesh_coordinator(void *arg);
static void task_ble_central(void *arg);
static void task_cloud_uplink(void *arg);
static void task_edge_ml(void *arg);
static void task_display(void *arg);
static void task_alert_manager(void *arg);
static void handle_packet(const pkt_header_t *hdr, const uint8_t *payload, uint16_t plen);
static void update_alert_zone(alert_zone_t new_zone);
static void trigger_alert(alert_zone_t zone, const char *message);
static void rgb_led_set(uint8_t r, uint8_t g, uint8_t b);
static void buzzer_beep(uint16_t freq_hz, uint16_t duration_ms);

/* ── App Entry ─────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "=== AsthmaSync Hub v%d.%d ===",
             PROTO_VERSION_MAJOR, PROTO_VERSION_MINOR);
    ESP_LOGI(TAG, "ESP32-S3 — %d MB flash, %d MB PSRAM",
             spi_flash_get_chip_size() / (1024*1024),
             (int)(esp_psram_get_size() / (1024*1024)));

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Create synchronization primitives */
    s_event_queue = xQueueCreate(32, sizeof(event_payload_t));
    s_zone_mutex  = xMutexCreate();

    /* Initialize GPIO */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_BTN_ACK) | (1ULL << GPIO_BTN_SILENCE) | (1ULL << GPIO_BTN_PAIR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);

    /* Initialize RGB LED (WS2812 via RMT — simplified here) */
    rgb_led_set(0, 0, 50);  /* dim blue: booting */

    /* Initialize Sub-GHz radio */
    ESP_LOGI(TAG, "Initializing SX1262 radio...");
    if (radio_init() != 0) {
        ESP_LOGE(TAG, "Radio init failed!");
    }

    /* Initialize edge ML */
    ESP_LOGI(TAG, "Loading edge ML models...");
    if (edge_ml_init() != 0) {
        ESP_LOGE(TAG, "Edge ML init failed — wheeze/actuation inference unavailable");
    } else {
        ESP_LOGI(TAG, "Edge ML models loaded (wheeze CNN + actuation RF)");
    }

    /* Initialize MQTT */
    ESP_LOGI(TAG, "Connecting to cloud...");
    mqtt_init();

    /* Create FreeRTOS tasks */
    xTaskCreatePinnedToCore(task_mesh_coordinator, "mesh",   8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(task_ble_central,      "ble",    6144, NULL, 6, NULL, 0);
    xTaskCreatePinnedToCore(task_cloud_uplink,     "cloud",  6144, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(task_edge_ml,           "eml",    8192, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(task_display,          "disp",   4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(task_alert_manager,    "alert",  3072, NULL, 7, NULL, 1);

    rgb_led_set(0, 50, 0);  /* green: ready */
    ESP_LOGI(TAG, "Hub started — all tasks launched");

    /* Main loop: heartbeat */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_SEC * 1000));
        ESP_LOGI(TAG, "Heartbeat — zone=%d, nodes=%d, rescue/wk=%d",
                 s_current_zone, s_node_count, s_rescue_count_week);
    }
}

/* ── Sub-GHz Mesh Coordinator ─────────────────────────── */
static void task_mesh_coordinator(void *arg)
{
    ESP_LOGI(TAG, "Mesh coordinator started");

    while (1) {
        /* Run TDMA superframe:
           - Slot 0: send beacon + time sync
           - Slots 1-8: listen for node packets */
        tdma_hub_superframe();

        /* Process any received packets */
        uint8_t rx_buf[PKT_MAX_SIZE];
        int8_t rssi, snr;

        int n = radio_recv(rx_buf, sizeof(rx_buf), &rssi, &snr);
        if (n > 0) {
            pkt_header_t hdr;
            uint8_t payload[PKT_MAX_PAYLOAD];
            uint16_t plen;

            if (proto_unpack(rx_buf, n, &hdr, payload, &plen) == 0) {
                ESP_LOGI(TAG, "RX mesh: type=0x%02X src=0x%04X len=%d rssi=%d",
                         hdr.msg_type, hdr.src_id, plen, rssi);
                handle_packet(&hdr, payload, plen);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));  /* yield */
    }
}

/* ── BLE Central ───────────────────────────────────────── */
static void task_ble_central(void *arg)
{
    ESP_LOGI(TAG, "BLE central started — scanning for Inhaler Tag + Wheeze Band");

    /* In production: use NimBLE GAP + GATT client
       - Scan for devices advertising AsthmaSync service UUID
       - Connect to Inhaler Tag + Wheeze Band
       - Subscribe to telemetry characteristic (notify)
       - Forward packets to handle_packet() */

    while (1) {
        /* Simulated BLE receive */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ── Cloud Uplink ──────────────────────────────────────── */
static void task_cloud_uplink(void *arg)
{
    ESP_LOGI(TAG, "Cloud uplink started");

    while (1) {
        if (mqtt_get_state() == MQTT_STATE_CONNECTED) {
            mqtt_flush_queue();
        }
        vTaskDelay(pdMS_TO_TICKS(CLOUD_UPLOAD_INTERVAL_SEC * 1000));
    }
}

/* ── Edge ML Processor ─────────────────────────────────── */
static void task_edge_ml(void *arg)
{
    ESP_LOGI(TAG, "Edge ML processor started");

    while (1) {
        /* Wait for audio feature packets from Wheeze Band */
        event_payload_t evt;
        if (xQueueReceive(s_event_queue, &evt, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (evt.event_type == EVT_WHEEZE_DETECTED) {
                /* The Wheeze Band already ran a pre-classifier.
                   Hub runs the full 22-class CNN on the mel-spectrogram. */
                ESP_LOGI(TAG, "Processing wheeze event through full CNN...");

                /* In production: retrieve mel-spectrogram from cache,
                   run edge_ml_classify_wheeze(), publish result */
                float probs[22];
                int best_class;
                /* edge_ml_classify_wheeze(mel_data, mel_len, probs, &best_class); */

                /* If wheeze class detected → trigger alert based on zone */
                /* update_alert_zone(new_zone); */
            }
        }
    }
}

/* ── Display ───────────────────────────────────────────── */
static void task_display(void *arg)
{
    ESP_LOGI(TAG, "Display task started");

    while (1) {
        /* In production: render to ILI9341 via LVGL or raw framebuffer
           - Zone indicator (green/yellow/red circle)
           - Current PM2.5, CO₂, VOC
           - Last rescue inhaler use
           - Last wheeze event
           - Connectivity status */

        xSemaphoreTake(s_zone_mutex, portMAX_DELAY);
        alert_zone_t zone = s_current_zone;
        xSemaphoreGive(s_zone_mutex);

        switch (zone) {
        case ZONE_GREEN:  rgb_led_set(0, 50, 0); break;
        case ZONE_YELLOW: rgb_led_set(50, 50, 0); break;
        case ZONE_RED:    rgb_led_set(50, 0, 0); break;
        }

        vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_MS));
    }
}

/* ── Alert Manager ────────────────────────────────────── */
static void task_alert_manager(void *arg)
{
    ESP_LOGI(TAG, "Alert manager started");

    while (1) {
        event_payload_t evt;
        if (xQueueReceive(s_event_queue, &evt, portMAX_DELAY) == pdTRUE) {
            switch (evt.event_type) {
            case EVT_SPO2_LOW:
                trigger_alert(ZONE_RED, "SpO2 below 92% — seek medical attention");
                break;
            case EVT_HRV_DROP:
                trigger_alert(ZONE_YELLOW, "HRV dropped below baseline — stress/exertion");
                break;
            case EVT_PM25_HIGH:
                trigger_alert(ZONE_YELLOW, "PM2.5 above 35 µg/m³ — ventilate room");
                break;
            case EVT_VOC_HIGH:
                trigger_alert(ZONE_YELLOW, "VOC level high — check air purifier");
                break;
            case EVT_ACTUATION:
                s_rescue_count_week++;
                ESP_LOGI(TAG, "Rescue inhaler used (count this week: %d)",
                         s_rescue_count_week);
                if (s_rescue_count_week >= THRESH_RESCUE_RED) {
                    trigger_alert(ZONE_RED, "Rescue use > 4×/week — contact doctor");
                } else if (s_rescue_count_week >= THRESH_RESCUE_YELLOW) {
                    trigger_alert(ZONE_YELLOW, "Rescue use > 2×/week — review action plan");
                }
                break;
            case EVT_FALL:
                trigger_alert(ZONE_RED, "Fall detected — check on patient");
                break;
            case EVT_BUTTON_SOS:
                trigger_alert(ZONE_RED, "SOS button pressed — emergency");
                break;
            default:
                break;
            }
        }
    }
}

/* ── Packet Handler ────────────────────────────────────── */
static void handle_packet(const pkt_header_t *hdr, const uint8_t *payload, uint16_t plen)
{
    if (!hdr || !payload || plen == 0)
        return;

    switch (hdr->msg_type) {
    case MSG_TYPE_JOIN_REQ: {
        /* Assign TDMA slot to new node */
        uint8_t slot;
        tdma_assign_slot(hdr->src_id, &slot);
        s_node_count++;
        ESP_LOGI(TAG, "Node joined: type=0x%02X id=0x%04X slot=%d",
                 hdr->src_type, hdr->src_id, slot);
        break;
    }
    case MSG_TYPE_TELEMETRY: {
        /* Build JSON + publish to cloud */
        char json[256];
        if (mqtt_build_telemetry_json(hdr, payload, plen, json, sizeof(json)) == 0) {
            mqtt_publish(MQTT_TOPIC_TELEMETRY, json, strlen(json), 1);
        }
        break;
    }
    case MSG_TYPE_EVENT: {
        if (plen >= sizeof(event_payload_t)) {
            event_payload_t evt;
            memcpy(&evt, payload, sizeof(evt));
            xQueueSend(s_event_queue, &evt, 0);
        }
        break;
    }
    default:
        break;
    }
}

/* ── Alert Zone Update ─────────────────────────────────── */
static void update_alert_zone(alert_zone_t new_zone)
{
    xSemaphoreTake(s_zone_mutex, portMAX_DELAY);
    alert_zone_t old = s_current_zone;
    s_current_zone = new_zone;
    xSemaphoreGive(s_zone_mutex);

    if (new_zone != old) {
        ESP_LOGW(TAG, "Zone changed: %d → %d", old, new_zone);
    }
}

/* ── Trigger Alert ─────────────────────────────────────── */
static void trigger_alert(alert_zone_t zone, const char *message)
{
    ESP_LOGW(TAG, "ALERT [zone=%d]: %s", zone, message);

    /* Visual: RGB LED */
    switch (zone) {
    case ZONE_RED:
        rgb_led_set(255, 0, 0);
        buzzer_beep(3000, 500);  /* 3 kHz, 500 ms */
        break;
    case ZONE_YELLOW:
        rgb_led_set(255, 165, 0);
        buzzer_beep(2000, 200);
        break;
    default:
        rgb_led_set(0, 255, 0);
        break;
    }

    /* Publish event to cloud */
    char json[256];
    snprintf(json, sizeof(json),
             "{\"type\":\"alert\",\"zone\":%d,\"message\":\"%s\"}",
             zone, message);
    mqtt_publish(MQTT_TOPIC_EVENTS, json, strlen(json), 1);
}

/* ── RGB LED (WS2812 via RMT — simplified) ─────────────── */
static void rgb_led_set(uint8_t r, uint8_t g, uint8_t b)
{
    /* In production: use RMT peripheral to drive WS2812 protocol
       24-bit GRB data at 800 kHz */
    (void)r; (void)g; (void)b;
    gpio_set_level(GPIO_RGB_LED, 0);
}

/* ── Buzzer ────────────────────────────────────────────── */
static void buzzer_beep(uint16_t freq_hz, uint16_t duration_ms)
{
    /* In production: use LEDC PWM on GPIO_BUZZER */
    (void)freq_hz; (void)duration_ms;
    gpio_set_level(GPIO_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    gpio_set_level(GPIO_BUZZER, 0);
}