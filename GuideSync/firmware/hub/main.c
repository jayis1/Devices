/*
 * GuideSync — Vision Hub / Gateway Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Hub coordinates the BLE star network, bridges to the cloud
 * via Wi-Fi/MQTT with 4G LTE cellular backup, runs heavier OCR
 * inference (EAST + CRNN) for TextReader, manages OTA firmware
 * distribution, and dispatches emergency alerts (fall, SOS) via
 * SMS + 911 call through the SIM7000 4G LTE module.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

#include "../common/protocol.h"
#include "../common/ble_mesh.h"
#include "../common/config.h"

static const char *TAG = "GuideSync-Hub";

/* === Global state === */
static gs_ble_ctx_t g_ble;
static QueueHandle_t g_telemetry_queue;
static QueueHandle_t g_command_queue;
static SemaphoreHandle_t g_ble_mutex;
static uint8_t g_node_table[GS_MAX_NODES];

/* Navigation state */
static uint8_t g_nav_active = 0;
static uint8_t g_nav_current_step = 0;
static uint8_t g_nav_total_steps = 0;

/* Emergency state */
static uint8_t g_emergency_active = 0;
static uint8_t g_emergency_cancel_window = 0;

/* === BLE Interface (ESP32-S3 NimBLE) === */
static void ble_init(void) { /* NimBLE init in production */ }
static void ble_advert_start(void) { /* Hub doesn't advertise */ }
static void ble_advert_stop(void) { }
static void ble_scan_start(void)
{
    /* Hub scans for peripherals during join phase */
    esp_ble_gap_start_scanning(10); /* 10 second scan */
}
static void ble_scan_stop(void) { esp_ble_gap_stop_scanning(); }
static int ble_connect(uint8_t *peer_addr)
{
    /* In production: esp_ble_gattc_open() */
    return 0;
}
static int ble_send(const uint8_t *data, uint8_t len)
{
    /* In production: esp_ble_gattc_write() to peripheral */
    ESP_LOGI(TAG, "BLE TX %d bytes", len);
    return len;
}
static int ble_recv(uint8_t *buf, uint8_t max_len, uint32_t timeout_ms)
{
    /* In production: GATT notification callback fills buffer */
    vTaskDelay(pdMS_TO_TICKS(timeout_ms > 100 ? 100 : timeout_ms));
    return 0; /* No data (stub) */
}
static void ble_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static void ble_disconnect(void) { /* esp_ble_gattc_close() */ }

static const gs_ble_interface_t g_ble_iface = {
    .init = ble_init,
    .advert_start = ble_advert_start,
    .advert_stop = ble_advert_stop,
    .scan_start = ble_scan_start,
    .scan_stop = ble_scan_stop,
    .connect = ble_connect,
    .send = ble_send,
    .recv = ble_recv,
    .delay_ms = ble_delay_ms,
    .disconnect = ble_disconnect,
};

/* === I2C for BME280 + DS3231 === */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = HUB_GPIO_BME_SDA,
        .scl_io_num = HUB_GPIO_BME_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

static void read_bme280(float *temp, float *humidity, float *pressure)
{
    /* Simplified BME280 read — production uses calibration coeffs */
    *temp = 22.5;
    *humidity = 45.0;
    *pressure = 1013.0;
}

/* === Status LEDs === */
static void set_led(uint8_t r, uint8_t g, uint8_t b)
{
    /* SK6812 via RMT peripheral in production */
    (void)r; (void)g; (void)b;
}

/* === Cellular Backup (SIM7000) === */
static void cellular_init(void)
{
    gpio_set_direction(HUB_GPIO_CELL_PWR, GPIO_MODE_OUTPUT);
    gpio_set_level(HUB_GPIO_CELL_PWR, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(HUB_GPIO_CELL_PWR, 1);
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level(HUB_GPIO_CELL_PWR, 0);
    ESP_LOGI(TAG, "SIM7000 4G LTE module initialized");
}

static int cellular_send_sms(const char *number, const char *message)
{
    /* Production: AT+CMGS="<number>" then message + Ctrl-Z */
    ESP_LOGI(TAG, "SMS to %s: %s", number, message);
    return 0;
}

static int cellular_call_911(const char *gps_coords)
{
    /* Production: ATD911; + automated voice via TTS or pre-recorded */
    ESP_LOGW(TAG, "CALLING 911 — GPS: %s", gps_coords);
    return 0;
}

static int cellular_get_gps(char *coords, size_t len)
{
    /* Production: AT+CGNSINF → parse NMEA */
    snprintf(coords, len, "40.7128,-74.0060");
    return 0;
}

/* === Emergency Dispatch === */
static void dispatch_emergency(const char *alert_type, uint8_t severity)
{
    char gps[64];
    cellular_get_gps(gps, sizeof(gps));

    char message[256];
    snprintf(message, sizeof(message),
             "GuideSync EMERGENCY: %s. User GPS: %s. "
             "This is an automated alert from GuideSync vision assistance system.",
             alert_type, gps);

    /* Send SMS to all emergency contacts (production: load from NVS) */
    cellular_send_sms("contact1", message);
    cellular_send_sms("contact2", message);

    /* Call 911 for fall or SOS */
    if (severity == GS_SEV_EMERGENCY) {
        cellular_call_911(gps);
    }

    /* Buzzer */
    gpio_set_level(HUB_GPIO_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(3000));
    gpio_set_level(HUB_GPIO_BUZZER, 0);

    g_emergency_active = 1;
    g_emergency_cancel_window = FALL_CANCEL_WINDOW_S;

    ESP_LOGW(TAG, "Emergency dispatched: %s (severity %d)", alert_type, severity);
}

/* === MQTT Client (simplified) === */
static void mqtt_publish_telemetry(const gs_message_t *msg)
{
    char json[512];
    uint8_t node_id = msg->header.src;
    uint8_t subtype = msg->payload[0];

    if (subtype == GS_TELEM_GLASSES) {
        uint8_t obj_class = msg->payload[5];
        uint8_t obj_dist = msg->payload[6];
        uint8_t crosswalk = msg->payload[11];
        uint8_t signal = msg->payload[12];
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"glasses\",\"obstacle_class\":%d,"
            "\"obstacle_dist_dm\":%d,\"crosswalk\":%d,\"signal\":%d,"
            "\"battery\":%.2f}",
            node_id, obj_class, obj_dist, crosswalk, signal,
            msg->payload[1] / 100.0);
    } else if (subtype == GS_TELEM_CANE) {
        uint8_t us_dist = msg->payload[2];
        uint8_t dropoff = msg->payload[5];
        uint8_t stair = msg->payload[6];
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"cane\",\"us_dist_dm\":%d,"
            "\"dropoff\":%d,\"stairs\":%d,\"battery\":%.2f}",
            node_id, us_dist, dropoff, stair, msg->payload[1] / 100.0);
    } else if (subtype == GS_TELEM_BAND) {
        uint8_t nav_dir = msg->payload[7];
        uint8_t sos = msg->payload[9];
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"band\",\"nav_dir\":%d,"
            "\"sos_armed\":%d,\"battery\":%.2f}",
            node_id, nav_dir, sos, msg->payload[1] / 100.0);
    } else {
        snprintf(json, sizeof(json), "{\"node\":%d,\"type\":\"unknown\"}", node_id);
    }

    ESP_LOGI(TAG, "MQTT pub: %s", json);
    /* esp_mqtt_client_publish(mqtt_client, topic, json, 0, 1, 0); */
}

/* === Command peripherals === */
static void send_nav_to_band(uint8_t direction, uint8_t distance_m,
                             uint16_t landmark_id, uint8_t eta_min)
{
    gs_message_t nav;
    gs_build_nav_update(&nav, GS_HUB_NODE_ID, GS_NODE_BAND, g_ble.msg_seq++,
                        direction, distance_m, landmark_id, eta_min,
                        g_nav_current_step);
    gs_ble_send(&g_ble, &nav);
    ESP_LOGI(TAG, "NAV to band: dir=%d dist=%dm landmark=%d eta=%dm",
             direction, distance_m, landmark_id, eta_min);
}

/* === BLE Coordinator Task === */
static void ble_task(void *arg)
{
    if (gs_ble_init(&g_ble, GS_NODE_HUB, &g_ble_iface) != 0) {
        ESP_LOGE(TAG, "BLE init failed");
        vTaskDelete(NULL);
    }

    g_ble.node_id = GS_HUB_NODE_ID;
    g_ble.joined = 1;

    ESP_LOGI(TAG, "Hub BLE coordinator started (node_id=0)");

    /* In production: hub scans for advertising peripherals, connects,
     * and manages GATT connections. Here we wait for messages. */
    gs_message_t msg;
    while (1) {
        if (gs_ble_recv(&g_ble, &msg, 2000) == 0) {
            switch (msg.header.type) {
                case GS_MSG_JOIN_REQ: {
                    uint8_t new_id;
                    if (gs_ble_hub_assign_id(&g_ble, msg.payload[0], &new_id) == 0) {
                        gs_message_t ack;
                        memset(&ack, 0, sizeof(ack));
                        ack.header.sync[0] = GS_SYNC0;
                        ack.header.sync[1] = GS_SYNC1;
                        ack.header.src = GS_HUB_NODE_ID;
                        ack.header.dst = new_id;
                        ack.header.type = GS_MSG_JOIN_ACK;
                        ack.header.msg_id = g_ble.msg_seq++;
                        ack.payload[0] = new_id;
                        ack.payload_len = 1;
                        gs_ble_send(&g_ble, &ack);

                        g_node_table[new_id] = msg.payload[0];
                        ESP_LOGI(TAG, "Node joined: id=%d type=%d",
                                 new_id, msg.payload[0]);
                    }
                    break;
                }

                case GS_MSG_TELEMETRY:
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    break;

                case GS_MSG_FALL_ALERT: {
                    uint8_t impact = msg.payload[0];
                    uint8_t stillness = msg.payload[5];
                    ESP_LOGW(TAG, "FALL ALERT from node %d: impact=%d mg, stillness=%ds",
                             msg.header.src, impact, stillness);

                    /* Send FALL haptic to band */
                    gs_message_t haptic_cmd;
                    uint8_t cmd_data[1] = {GS_ALERT_FALL};
                    gs_build_command(&haptic_cmd, GS_HUB_NODE_ID, GS_NODE_BAND,
                                     g_ble.msg_seq++, GS_CMD_BEEP, cmd_data, 1);
                    gs_ble_send(&g_ble, &haptic_cmd);

                    /* Dispatch emergency */
                    dispatch_emergency("Fall detected", GS_SEV_EMERGENCY);

                    xQueueSend(g_telemetry_queue, &msg, 0);
                    break;
                }

                case GS_MSG_SOS_ALERT: {
                    uint8_t duration = msg.payload[1];
                    ESP_LOGW(TAG, "SOS ALERT from node %d: press=%ds",
                             msg.header.src, duration);
                    dispatch_emergency("SOS button pressed", GS_SEV_EMERGENCY);
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    break;
                }

                case GS_MSG_SCENE_DESC: {
                    uint8_t obj_count = msg.payload[0];
                    uint8_t crosswalk = msg.payload[1];
                    ESP_LOGI(TAG, "Scene from glasses: %d objects, crosswalk=%d",
                             obj_count, crosswalk);
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    break;
                }

                case GS_MSG_OCR_REQUEST:
                    /* Run OCR on hub (EAST + CRNN) and return text */
                    ESP_LOGI(TAG, "OCR request from glasses (node %d)",
                             msg.header.src);
                    /* Production: run EAST+CRNN inference on image data */
                    gs_message_t ocr_result;
                    const char *sample_text = "EXIT";
                    uint8_t text_len = (uint8_t)strlen(sample_text);
                    memset(&ocr_result, 0, sizeof(ocr_result));
                    ocr_result.header.sync[0] = GS_SYNC0;
                    ocr_result.header.sync[1] = GS_SYNC1;
                    ocr_result.header.src = GS_HUB_NODE_ID;
                    ocr_result.header.dst = msg.header.src;
                    ocr_result.header.type = GS_MSG_OCR_RESULT;
                    ocr_result.header.msg_id = g_ble.msg_seq++;
                    ocr_result.payload[0] = text_len;
                    memcpy(&ocr_result.payload[1], sample_text, text_len);
                    ocr_result.payload_len = 1 + text_len;
                    gs_ble_send(&g_ble, &ocr_result);
                    break;

                case GS_MSG_BEACON_SCAN: {
                    uint8_t count = msg.payload[0];
                    ESP_LOGI(TAG, "Beacon scan from node %d: %d beacons",
                             msg.header.src, count);
                    /* Production: feed to NavNet for position estimation */
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    break;
                }

                case GS_MSG_ALERT: {
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    uint8_t alert_type = msg.payload[0];
                    uint8_t severity = msg.payload[1];
                    ESP_LOGW(TAG, "ALERT from node %d: type=%d sev=%d",
                             msg.header.src, alert_type, severity);
                    if (severity >= GS_SEV_CRITICAL) {
                        /* Forward critical alerts to cloud immediately */
                        char alert_msg[128];
                        snprintf(alert_msg, sizeof(alert_msg),
                                 "GuideSync alert type %d", alert_type);
                        cellular_send_sms("contact1", alert_msg);
                    }
                    break;
                }

                case GS_MSG_HEARTBEAT:
                    ESP_LOGI(TAG, "Heartbeat from node %d, rssi=%d",
                             msg.header.src, msg.payload[0]);
                    break;

                default:
                    ESP_LOGI(TAG, "Unknown msg type 0x%02X from %d",
                             msg.header.type, msg.header.src);
            }
        }

        /* Broadcast time sync every ~60 seconds */
        static uint32_t frame_count = 0;
        if (++frame_count % 30 == 0) {
            uint32_t epoch = (uint32_t)(esp_timer_get_time() / 1000000ULL);
            gs_ble_hub_time_sync(&g_ble, epoch);
        }
    }
}

/* === MQTT Task: Cloud Bridge === */
static void mqtt_task(void *arg)
{
    gs_message_t msg;
    while (1) {
        if (xQueueReceive(g_telemetry_queue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            mqtt_publish_telemetry(&msg);
        }

        /* Check for commands from cloud */
        gs_message_t cmd;
        if (xQueueReceive(g_command_queue, &cmd, 0) == pdTRUE) {
            xSemaphoreTake(g_ble_mutex, portMAX_DELAY);
            gs_ble_send(&g_ble, &cmd);
            xSemaphoreGive(g_ble_mutex);
        }
    }
}

/* === Emergency Cancel Task === */
static void emergency_task(void *arg)
{
    ESP_LOGI(TAG, "Emergency monitor task started");
    while (1) {
        if (g_emergency_active && g_emergency_cancel_window > 0) {
            g_emergency_cancel_window--;
            if (g_emergency_cancel_window == 0) {
                ESP_LOGI(TAG, "Emergency cancel window expired — alert dispatched");
                g_emergency_active = 0;
            }
        }

        /* Check for SOS cancel command */
        gs_message_t cmd;
        if (xQueueReceive(g_command_queue, &cmd, 0) == pdTRUE) {
            if (cmd.header.type == GS_MSG_COMMAND &&
                cmd.payload[0] == GS_CMD_SOS_CANCEL) {
                ESP_LOGI(TAG, "Emergency cancelled by user");
                g_emergency_active = 0;
                g_emergency_cancel_window = 0;
            } else {
                /* Re-queue for mqtt_task */
                xQueueSend(g_command_queue, &cmd, 0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* === Navigation Task ===
 * In production: receives beacon scans, runs NavNet LSTM, computes
 * A* route, sends turn-by-turn nav updates to haptic band.
 */
static void nav_task(void *arg)
{
    ESP_LOGI(TAG, "Navigation task started");
    while (1) {
        if (g_nav_active) {
            /* Production: check position vs route, send nav updates */
            ESP_LOGI(TAG, "Nav active: step %d/%d",
                     g_nav_current_step, g_nav_total_steps);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* === Weather/Health Task === */
static void health_task(void *arg)
{
    float temp, hum, pres;
    while (1) {
        read_bme280(&temp, &hum, &pres);
        ESP_LOGI(TAG, "Ambient: %.1fC, %.1f%% RH, %.0f hPa", temp, hum, pres);
        vTaskDelay(pdMS_TO_TICKS(300000)); /* 5 minutes */
    }
}

/* === Status Task === */
static void status_task(void *arg)
{
    uint8_t state = 0;
    while (1) {
        state = !state;
        if (g_emergency_active) {
            set_led(state ? 100 : 0, 0, 0); /* Red blink */
        } else {
            set_led(0, state ? 50 : 0, 0); /* Green dim blink */
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "GuideSync Vision Hub starting...");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* GPIO */
    gpio_set_direction(HUB_GPIO_BUZZER, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_CELL_PWR, GPIO_MODE_OUTPUT);

    /* I2C */
    i2c_init();

    /* Cellular */
    cellular_init();

    /* Queues + mutex */
    g_telemetry_queue = xQueueCreate(64, sizeof(gs_message_t));
    g_command_queue = xQueueCreate(16, sizeof(gs_message_t));
    g_ble_mutex = xSemaphoreCreateMutex();

    /* Tasks */
    xTaskCreate(ble_task, "ble", 8192, NULL, 5, NULL);
    xTaskCreate(mqtt_task, "mqtt", 6144, NULL, 4, NULL);
    xTaskCreate(emergency_task, "emergency", 4096, NULL, 6, NULL);
    xTaskCreate(nav_task, "nav", 6144, NULL, 3, NULL);
    xTaskCreate(health_task, "health", 3072, NULL, 2, NULL);
    xTaskCreate(status_task, "status", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "GuideSync Vision Hub ready.");
}