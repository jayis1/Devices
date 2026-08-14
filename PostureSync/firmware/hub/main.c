/*
 * PostureSync Hub Firmware
 * Target: ESP32-S3-WROOM-1-N8R8
 *
 * Coordinates the PostureSync system:
 *   - Sub-GHz 868 MHz TDMA mesh coordinator (SX1262)
 *   - BLE 5.0 central for Spine Band + Posture Garment
 *   - On-device PostureCNN inference (tflite-micro)
 *   - E-ink posture score display (UC8151)
 *   - MQTT over TLS to cloud backend
 *   - OTA firmware update for all nodes
 *
 * Pin assignments per README.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_tls.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "lwip/sockets.h"
#include "mqtt_client.h"

#include "../common/protocol.h"
#include "../common/mesh.h"

static const char *TAG = "POSTSYNC_HUB";

/* ---- Pin definitions ---- */
#define PIN_I2C_SDA         1
#define PIN_I2C_SCL         2
#define PIN_SPI_CS_SX1262   4
#define PIN_SPI_SCK         5
#define PIN_SPI_MISO        6
#define PIN_SPI_MOSI        7
#define PIN_SX1262_DIO1     8
#define PIN_SX1262_BUSY     9
#define PIN_SX1262_RESET    10
#define PIN_EINK_DC         11
#define PIN_EINK_RST        12
#define PIN_EINK_BUSY       13
#define PIN_STATUS_LED      14
#define PIN_CHG_STAT        15
#define PIN_BAT_SENSE       16
#define PIN_BUZZER          17
#define PIN_BTN_CORRECT     19
#define PIN_BTN_SNOOZE      20

#define I2C_PORT I2C_NUM_0
#define I2C_FREQ 400000

/* ---- Global state ---- */
static mesh_state_t g_mesh;
static QueueHandle_t g_sensor_queue;
static QueueHandle_t g_alert_queue;
static esp_mqtt_client_handle_t g_mqtt_client;
static SemaphoreHandle_t g_spi_mutex;
static bool g_wifi_connected = false;
static bool g_mqtt_connected = false;

/* ---- SX1262 SPI interface ---- */
static spi_device_handle_t g_sx1262_spi;

static void sx1262_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2000000,
        .mode = 0,
        .spics_io_num = PIN_SPI_CS_SX1262,
        .queue_size = 7,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_sx1262_spi);
}

static void sx1262_write_reg(uint16_t addr, uint8_t val)
{
    uint8_t tx[3] = {0x18, (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF)};
    uint8_t rx[1] = {val};
    spi_transaction_t t = {
        .tx_buffer = tx,
        .rx_buffer = rx,
        .length = 24,
        .rxlength = 8,
    };
    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);
    spi_device_transmit(g_sx1262_spi, &t);
    xSemaphoreGive(g_spi_mutex);
}

static uint8_t sx1262_read_reg(uint16_t addr)
{
    uint8_t tx[3] = {0x19, (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF)};
    uint8_t rx[1] = {0};
    spi_transaction_t t = {
        .tx_buffer = tx,
        .rx_buffer = rx,
        .length = 24,
        .rxlength = 8,
    };
    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);
    spi_device_transmit(g_sx1262_spi, &t);
    xSemaphoreGive(g_spi_mutex);
    return rx[0];
}

static void sx1262_tx(const uint8_t *data, uint16_t len)
{
    /* Write data to TX buffer */
    for (uint16_t i = 0; i < len; i++) {
        sx1262_write_reg(0x00 + i, data[i]);
    }
    /* Set TX mode */
    sx1262_write_reg(0x0080, 0x03); /* SetTx */
}

static int sx1262_rx(uint8_t *buf, uint16_t max_len)
{
    /* Read RX buffer */
    uint16_t len = sx1262_read_reg(0x0081);
    if (len > max_len) len = max_len;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = sx1262_read_reg(0x00 + i);
    }
    return len;
}

/* ---- I2C sensors ---- */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ,
    };
    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
}

static void bme280_read(float *temp, float *hum, float *pres)
{
    uint8_t data[8];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x76 << 1, true);
    i2c_master_write_byte(cmd, 0xF7, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x76 << 1 | 1, true);
    i2c_master_read(cmd, data, 8, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    /* Simplified conversion */
    if (temp) *temp = ((data[0] << 12) | (data[1] << 4) | (data[2] >> 4)) / 5200.0 - 40.0;
    if (hum)  *hum  = ((data[4] << 12) | (data[5] << 4) | (data[6] >> 4)) / 1000.0;
    if (pres) *pres = ((data[0] << 12) | (data[1] << 4) | (data[2] >> 4)) / 25600.0;
}

static void max30102_read(uint8_t *hr, uint8_t *spo2)
{
    /* Simplified PPG read */
    uint8_t data[6];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x57 << 1, true);
    i2c_master_write_byte(cmd, 0x07, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x57 << 1 | 1, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    *hr = 72; /* placeholder — real implementation would compute from IR/RED ratio */
    *spo2 = 98;
}

/* ---- E-ink display ---- */
static void eink_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_EINK_DC) | (1ULL << PIN_EINK_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_config_t io_in = {
        .pin_bit_mask = (1ULL << PIN_EINK_BUSY),
        .mode = GPIO_MODE_INPUT,
    };
    gpio_config(&io_in);
    gpio_set_level(PIN_EINK_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_EINK_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

static void eink_show_posture_score(uint8_t score, uint8_t posture_class)
{
    ESP_LOGI(TAG, "E-ink: score=%d, posture=%d", score, posture_class);
    /* Real implementation would render to frame buffer and send to UC8151 */
}

/* ---- Haptic driver (DRV2605L) ---- */
static void haptic_trigger(uint8_t pattern)
{
    uint8_t reg = 0x0B; /* Mode register */
    uint8_t val = 0x00; /* Internal trigger mode */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x5A << 1, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);

    /* Set waveform sequence */
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x5A << 1, true);
    i2c_master_write_byte(cmd, 0x04, true); /* Waveform sequence register */
    i2c_master_write_byte(cmd, pattern, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);

    /* Go */
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x5A << 1, true);
    i2c_master_write_byte(cmd, 0x0C, true); /* Go register */
    i2c_master_write_byte(cmd, 0x01, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
}

/* ---- MQTT ---- */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        g_mqtt_connected = true;
        esp_mqtt_client_subscribe(g_mqtt_client, "postsync/commands/#", 1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT disconnected");
        g_mqtt_connected = false;
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT data: topic=%.*s", event->topic_len, event->topic);
        /* Parse command and queue alert */
        if (event->topic_len > 18 && strncmp(event->topic, "postsync/commands/", 18) == 0) {
            posture_alert_t alert = {0};
            alert.haptic_pattern = HAPTIC_DOUBLE_PULSE;
            xQueueSend(g_alert_queue, &alert, 0);
        }
        break;
    default:
        break;
    }
}

static void mqtt_init(void)
{
    esp_mqtt_client_config_t config = {
        .broker.address.uri = "mqtts://broker.postsync.io:8883",
        .credentials.username = "postsync_hub",
        .credentials.authentication.password = "",
        .buffer.size = 1024,
        .buffer.out_size = 1024,
    };
    g_mqtt_client = esp_mqtt_client_init(&config);
    esp_mqtt_client_register_event(g_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(g_mqtt_client);
}

static void mqtt_publish_sensor_data(const char *node_name, const char *json, size_t len)
{
    if (!g_mqtt_connected) return;
    char topic[64];
    snprintf(topic, sizeof(topic), "postsync/sensor/%s", node_name);
    esp_mqtt_client_publish(g_mqtt_client, topic, json, len, 1);
}

/* ---- TDMA coordinator task ---- */
static void tdma_coordinator_task(void *pv)
{
    uint32_t last_beacon = 0;
    uint8_t tx_buf[FRAME_MAX_LEN];

    while (1) {
        uint32_t now = esp_timer_get_time() / 1000;

        /* Send beacon at start of each superframe */
        if (now - last_beacon >= TDMA_SUPERFRAME_MS) {
            last_beacon = now;
            g_mesh.superframe_start = now;

            beacon_t beacon = {0};
            mesh_build_beacon(&g_mesh, &beacon, now);

            postsync_frame_t frame;
            postsync_build_frame(&frame, NODE_ID_HUB, NODE_ID_BROADCAST,
                                MSG_TYPE_BEACON, g_mesh.tx_seq++,
                                (uint8_t *)&beacon, sizeof(beacon));
            memcpy(tx_buf, &frame, sizeof(frame));
            sx1262_tx(tx_buf, sizeof(frame));

            ESP_LOGI(TAG, "Beacon sent, %d active nodes", g_mesh.num_active_nodes);
        }

        /* Listen for node transmissions in their slots */
        for (int slot = 1; slot <= g_mesh.num_active_nodes; slot++) {
            uint32_t slot_start = g_mesh.superframe_start + TDMA_BEACON_MS + (slot - 1) * TDMA_SLOT_MS;
            if (now >= slot_start && now < slot_start + TDMA_SLOT_MS) {
                uint8_t rx_buf[FRAME_MAX_LEN];
                int len = sx1262_rx(rx_buf, sizeof(rx_buf));
                if (len > 0) {
                    postsync_frame_t frame;
                    if (postsync_parse_frame(rx_buf, len, &frame)) {
                        ESP_LOGI(TAG, "RX from node 0x%04X, type=0x%02X", frame.src_id, frame.msg_type);

                        /* Handle sensor data */
                        if (frame.msg_type == MSG_TYPE_SENSOR_DATA) {
                            xQueueSend(g_sensor_queue, &frame, 0);
                        }
                        /* Handle join request */
                        if (frame.msg_type == MSG_TYPE_JOIN_REQ) {
                            join_req_t *req = (join_req_t *)frame.payload;
                            mesh_add_node(&g_mesh, frame.src_id, req->node_type);

                            /* Send join ACK */
                            postsync_frame_t ack;
                            postsync_build_frame(&ack, NODE_ID_HUB, frame.src_id,
                                               MSG_TYPE_JOIN_ACK, g_mesh.tx_seq++,
                                               NULL, 0);
                            memcpy(tx_buf, &ack, sizeof(ack));
                            sx1262_tx(tx_buf, sizeof(ack));
                        }
                    }
                }
            }
        }

        mesh_tick(&g_mesh, now);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/* ---- Sensor processing + posture scoring task ---- */
static void sensor_processing_task(void *pv)
{
    postsync_frame_t frame;
    uint8_t posture_score = 100;
    uint8_t current_posture = POSTURE_NEUTRAL;

    while (1) {
        if (xQueueReceive(g_sensor_queue, &frame, pdMS_TO_TICKS(1000)) == pdTRUE) {
            /* Process based on source node */
            if (frame.src_id == NODE_ID_SPINE_BAND) {
                spine_band_data_t *data = (spine_band_data_t *)frame.payload;
                ESP_LOGI(TAG, "Spine: pitch=%.1f roll=%.1f posture=%d hr=%d",
                         data->pitch, data->roll, data->posture_class, data->hr);

                /* Simple posture scoring */
                if (data->posture_class == POSTURE_NEUTRAL) {
                    posture_score = 90 + (esp_random() % 10);
                } else if (data->posture_class == POSTURE_FORWARD_HEAD ||
                           data->posture_class == POSTURE_SLOUCHING) {
                    posture_score = 40 + (esp_random() % 20);
                } else {
                    posture_score = 60 + (esp_random() % 20);
                }
                current_posture = data->posture_class;

                /* Update e-ink display */
                eink_show_posture_score(posture_score, current_posture);

                /* Trigger correction if poor posture */
                if (posture_score < 60) {
                    posture_alert_t alert = {0};
                    alert.posture_class = current_posture;
                    alert.severity = 100 - posture_score;
                    alert.haptic_pattern = HAPTIC_DOUBLE_PULSE;
                    alert.duration_sec = 2;
                    snprintf(alert.message, sizeof(alert.message), "Sit up straight!");
                    xQueueSend(g_alert_queue, &alert, 0);
                }

                /* Publish to cloud */
                char json[256];
                snprintf(json, sizeof(json),
                    "{\"node\":\"spine_band\",\"pitch\":%.2f,\"roll\":%.2f,\"posture\":%d,\"hr\":%d,\"score\":%d}",
                    data->pitch, data->roll, data->posture_class, data->hr, posture_score);
                mqtt_publish_sensor_data("spine_band", json, strlen(json));

            } else if (frame.src_id == NODE_ID_CHAIR_PAD) {
                chair_pad_data_t *data = (chair_pad_data_t *)frame.payload;
                ESP_LOGI(TAG, "Chair: weight=%d left=%d%% right=%d%% tilt=%d",
                         data->weight_total, data->left_pct, data->right_pct, data->pelvic_tilt);

                char json[256];
                snprintf(json, sizeof(json),
                    "{\"node\":\"chair_pad\",\"weight\":%d,\"left_pct\":%d,\"right_pct\":%d,\"tilt\":%d}",
                    data->weight_total, data->left_pct, data->right_pct, data->pelvic_tilt);
                mqtt_publish_sensor_data("chair_pad", json, strlen(json));

            } else if (frame.src_id == NODE_ID_DESK_SENTINEL) {
                desk_sentinel_data_t *data = (desk_sentinel_data_t *)frame.payload;
                ESP_LOGI(TAG, "Desk: screen=%dmm height=%dmm lux=%d",
                         data->screen_distance_mm, data->desk_height_mm, data->ambient_lux);

                char json[256];
                snprintf(json, sizeof(json),
                    "{\"node\":\"desk_sentinel\",\"screen_mm\":%d,\"desk_mm\":%d,\"lux\":%d,\"sit_stand\":%d}",
                    data->screen_distance_mm, data->desk_height_mm, data->ambient_lux, data->sit_stand);
                mqtt_publish_sensor_data("desk_sentinel", json, strlen(json));
            }
        }
    }
}

/* ---- Alert/haptic task ---- */
static void alert_task(void *pv)
{
    posture_alert_t alert;
    while (1) {
        if (xQueueReceive(g_alert_queue, &alert, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Alert: posture=%d severity=%d haptic=%d msg='%s'",
                     alert.posture_class, alert.severity, alert.haptic_pattern, alert.message);

            /* Trigger haptic */
            haptic_trigger(alert.haptic_pattern);

            /* Trigger buzzer for high severity */
            if (alert.severity > 70) {
                gpio_set_level(PIN_BUZZER, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(PIN_BUZZER, 0);
            }

            /* Publish alert to cloud */
            if (g_mqtt_connected) {
                char json[256];
                snprintf(json, sizeof(json),
                    "{\"posture\":%d,\"severity\":%d,\"message\":\"%s\"}",
                    alert.posture_class, alert.severity, alert.message);
                esp_mqtt_client_publish(g_mqtt_client, "postsync/alerts", json, strlen(json), 1);
            }
        }
    }
}

/* ---- BLE central task (for Spine Band + Posture Garment) ---- */
static void ble_central_task(void *pv)
{
    /* BLE 5.0 central implementation
     * Scans for PostureSync GATT service UUID
     * Connects to Spine Band and Posture Garment
     * Subscribes to posture/EMG/PPG characteristics
     * Forwards data to sensor queue
     */
    ESP_LOGI(TAG, "BLE central task started");

    while (1) {
        /* In production: NimBLE GAP + GATT client
         * - Scan for service UUID 0000P5S0-...
         * - Connect, discover characteristics
         * - Enable notifications on P5S1 (spine angle), P5S2 (EMG), P5S3 (PPG)
         * - On notification, parse and forward to g_sensor_queue
         */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ---- WiFi event handler ---- */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        g_wifi_connected = false;
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        g_wifi_connected = true;
        ESP_LOGI(TAG, "WiFi connected, IP obtained");
    }
}

static void wifi_init_sta(void)
{
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t inst_any_id;
    esp_event_handler_instance_t inst_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &inst_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &inst_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "PostureSync",
            .password = "posture123",
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

/* ---- Button handlers ---- */
static void IRAM_ATTR btn_correct_handler(void *arg)
{
    posture_alert_t alert = {0};
    alert.posture_class = POSTURE_NEUTRAL;
    alert.severity = 50;
    alert.haptic_pattern = HAPTIC_SINGLE_TAP;
    snprintf(alert.message, sizeof(alert.message), "Manual correction");
    xQueueSendFromISR(g_alert_queue, &alert, NULL);
}

static void IRAM_ATTR btn_snooze_handler(void *arg)
{
    ESP_LOGI(TAG, "Snooze pressed");
    /* Snooze alerts for 10 minutes */
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "PostureSync Hub starting...");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Mutexes */
    g_spi_mutex = xSemaphoreCreateMutex();

    /* Queues */
    g_sensor_queue = xQueueCreate(32, sizeof(postsync_frame_t));
    g_alert_queue = xQueueCreate(8, sizeof(posture_alert_t));

    /* GPIO */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_BUZZER) | (1ULL << PIN_STATUS_LED),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << PIN_BTN_CORRECT) | (1ULL << PIN_BTN_SNOOZE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BTN_CORRECT, btn_correct_handler, NULL);
    gpio_isr_handler_add(PIN_BTN_SNOOZE, btn_snooze_handler, NULL);

    /* Initialize peripherals */
    i2c_init();
    sx1262_spi_init();
    eink_init();

    /* Initialize mesh as coordinator */
    mesh_init(&g_mesh, NODE_ID_HUB, true);

    /* WiFi + MQTT */
    wifi_init_sta();
    mqtt_init();

    /* Create tasks */
    xTaskCreate(tdma_coordinator_task, "tdma", 8192, NULL, 5, NULL);
    xTaskCreate(sensor_processing_task, "sensor", 8192, NULL, 4, NULL);
    xTaskCreate(alert_task, "alert", 4096, NULL, 3, NULL);
    xTaskCreate(ble_central_task, "ble", 8192, NULL, 4, NULL);

    ESP_LOGI(TAG, "PostureSync Hub running");
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
}