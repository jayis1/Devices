/*
 * StormSync — Hub / Gateway Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Hub coordinates the Sub-GHz mesh network, bridges to the cloud
 * via Wi-Fi/MQTT with 4G LTE cellular backup, runs local edge inference
 * (TFLite-Micro pump health anomaly detection), broadcasts flood status,
 * and manages OTA firmware distribution.
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
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/ledc.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "StormSync-Hub";

/* === Global state === */
static ss_mesh_ctx_t g_mesh;
static QueueHandle_t g_telemetry_queue;
static QueueHandle_t g_command_queue;
static SemaphoreHandle_t g_radio_mutex;
static uint8_t g_node_table[SS_SLOT_COUNT];

/* Current flood risk (broadcast to nodes) */
static uint8_t g_flood_score = 0;
static uint8_t g_flood_risk_level = 0;  /* 0=low, 1=moderate, 2=high, 3=critical */
static uint8_t g_storm_mode = 0;

/* === SX1262 SPI Interface (ESP32-S3) === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = HUB_GPIO_SX_MOSI,
        .miso_io_num = HUB_GPIO_SX_MISO,
        .sclk_io_num = HUB_GPIO_SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = HUB_GPIO_SX_NSS,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi_dev);
}

static void spi_cs_select(void) { /* Managed by SPI driver */ }
static void spi_cs_release(void) { /* Managed by SPI driver */ }
static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_transaction_t t = { .tx_buffer = &byte, .rx_buffer = &rx, .length = 8 };
    spi_device_polling_transmit(g_spi_dev, &t);
    return rx;
}
static void spi_reset(uint8_t assert) {
    gpio_set_level(HUB_GPIO_SX_RST, assert ? 0 : 1);
}
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(HUB_GPIO_SX_DIO1); }
static void spi_dio1_irq_enable(int enable) { (void)enable; }

static const ss_spi_interface_t g_spi_iface = {
    .init = spi_init,
    .cs_select = spi_cs_select,
    .cs_release = spi_cs_release,
    .transfer = spi_transfer,
    .reset = spi_reset,
    .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read,
    .dio1_irq_enable = spi_dio1_irq_enable,
};

/* === I²C for BME280 + DS3231 === */
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
    uint8_t buf[8];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x76 << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0xF7, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x76 << 1 | I2C_MASTER_READ, true);
    i2c_master_read(cmd, buf, 8, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    *pressure = (buf[0] << 12 | buf[1] << 4 | buf[2] >> 4) / 256.0;
    *temp = (buf[3] << 12 | buf[4] << 4 | buf[5] >> 4) / 100.0;
    *humidity = (buf[6] << 8 | buf[7]) / 1024.0 * 100.0;
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
    /* Initialize UART2 for SIM7000 4G LTE module
     * In production: AT command sequence to power on, check network,
     * and establish data connection as backup to Wi-Fi.
     */
    gpio_set_direction(HUB_GPIO_CELL_PWR, GPIO_MODE_OUTPUT);
    gpio_set_level(HUB_GPIO_CELL_PWR, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(HUB_GPIO_CELL_PWR, 1); /* Pulse to power on */
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level(HUB_GPIO_CELL_PWR, 0);
    ESP_LOGI(TAG, "SIM7000 cellular module initialized");
}

static int cellular_send_alert(const char *message)
{
    /* In production: send SMS or MQTT over 4G LTE
     * AT+CMGS="phone_number" followed by message
     * Or: establish PPP connection and publish MQTT
     */
    ESP_LOGI(TAG, "CELLULAR ALERT: %s", message);
    return 0;
}

/* === MQTT Client (simplified) === */
static void mqtt_publish_telemetry(const ss_message_t *msg)
{
    char json[512];
    uint8_t node_id = msg->header.src;
    uint8_t subtype = msg->payload[0];

    if (subtype == SS_TELEM_SUMP) {
        uint16_t water_level = msg->payload[2] | (msg->payload[3] << 8);
        uint16_t pump_current = msg->payload[4] | (msg->payload[5] << 8);
        uint8_t pump_status = msg->payload[6];
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"sump\",\"water_level_mm\":%d,"
            "\"pump_current_ma\":%d,\"pump_status\":%d}",
            node_id, water_level, pump_current, pump_status);
    } else if (subtype == SS_TELEM_SOIL) {
        uint16_t m15 = msg->payload[2] | (msg->payload[3] << 8);
        uint16_t m45 = msg->payload[4] | (msg->payload[5] << 8);
        uint16_t m90 = msg->payload[6] | (msg->payload[7] << 8);
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"soil\",\"moist_15\":%.1f,"
            "\"moist_45\":%.1f,\"moist_90\":%.1f}",
            node_id, m15/100.0, m45/100.0, m90/100.0);
    } else if (subtype == SS_TELEM_WEATHER) {
        int16_t temp = msg->payload[2] | (msg->payload[3] << 8);
        uint16_t pressure = msg->payload[6] | (msg->payload[7] << 8);
        uint16_t rain = msg->payload[12] | (msg->payload[13] << 8);
        uint8_t trend = msg->payload[14];
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"weather\",\"temp\":%.1f,"
            "\"pressure\":%.1f,\"rain_mm\":%.1f,\"pressure_trend\":%d}",
            node_id, temp/10.0, (pressure+8000)/10.0, rain*0.2, trend);
    } else if (subtype == SS_TELEM_ACTUATOR) {
        uint8_t valve = msg->payload[2];
        uint8_t pump = msg->payload[3];
        uint8_t flt = msg->payload[4];
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"actuator\",\"valve\":%d,"
            "\"pump_relay\":%d,\"float_switch\":%d}",
            node_id, valve, pump, flt);
    } else {
        snprintf(json, sizeof(json), "{\"node\":%d,\"type\":\"unknown\"}", node_id);
    }

    ESP_LOGI(TAG, "MQTT pub: %s", json);
    /* esp_mqtt_client_publish(mqtt_client, topic, json, 0, 1, 0); */
}

/* === Mesh Coordinator Task === */
static void mesh_task(void *arg)
{
    ss_radio_config_t radio_cfg = {
        .frequency = SS_NET_FREQ_HZ,
        .bandwidth = SS_NET_BW_HZ,
        .spreading_factor = SS_NET_SF,
        .coding_rate = SS_NET_CR,
        .preamble_len = SS_NET_PREAMBLE,
        .tx_power_dbm = SS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    if (ss_mesh_init(&g_mesh, SS_NODE_HUB, &g_spi_iface, &radio_cfg) != 0) {
        ESP_LOGE(TAG, "Mesh init failed");
        vTaskDelete(NULL);
    }

    g_mesh.node_id = SS_HUB_NODE_ID;
    g_mesh.tdma_slot = 0;
    g_mesh.joined = 1;

    ESP_LOGI(TAG, "Hub mesh coordinator started (node_id=0, slot=0)");

    ss_message_t msg;
    while (1) {
        if (ss_mesh_recv(&g_mesh, &msg, 2000) == 0) {
            switch (msg.header.type) {
                case SS_MSG_JOIN_REQ: {
                    uint8_t new_id, new_slot;
                    if (ss_mesh_hub_assign_slot(&g_mesh, msg.payload[0],
                                                &new_id, &new_slot) == 0) {
                        ss_message_t ack;
                        memset(&ack, 0, sizeof(ack));
                        ack.header.sync[0] = SS_SYNC0;
                        ack.header.sync[1] = SS_SYNC1;
                        ack.header.src = SS_HUB_NODE_ID;
                        ack.header.dst = new_id;
                        ack.header.type = SS_MSG_JOIN_ACK;
                        ack.header.msg_id = g_mesh.msg_seq++;
                        ack.payload[0] = new_id;
                        ack.payload[1] = new_slot;
                        ack.payload_len = 2;

                        uint8_t buf[SS_MAX_MSG];
                        size_t len = ss_encode(&ack, buf, sizeof(buf));
                        ss_radio_tx(buf, (uint8_t)len);

                        g_node_table[new_id] = new_slot;
                        ESP_LOGI(TAG, "Node joined: id=%d slot=%d type=%d",
                                 new_id, new_slot, msg.payload[0]);
                    }
                    break;
                }
                case SS_MSG_TELEMETRY:
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    break;

                case SS_MSG_ALERT: {
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    uint8_t alert_type = msg.payload[0];
                    uint8_t severity = msg.payload[1];
                    ESP_LOGW(TAG, "ALERT from node %d: type=%d sev=%d",
                             msg.header.src, alert_type, severity);

                    /* Critical alerts trigger immediate action */
                    if (severity >= 3) {
                        /* Send cellular alert if Wi-Fi is down */
                        cellular_send_alert("CRITICAL: Flood alert triggered!");
                    }
                    break;
                }

                case SS_MSG_HEARTBEAT:
                    ESP_LOGI(TAG, "Heartbeat from node %d, rssi=%d",
                             msg.header.src, msg.payload[1]);
                    break;

                case SS_MSG_FLOOD_STATUS:
                    /* Another node's flood status — relay to cloud */
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    break;

                default:
                    ESP_LOGI(TAG, "Unknown msg type 0x%02X from %d",
                             msg.header.type, msg.header.src);
            }
        }

        /* Broadcast time sync every ~60 seconds */
        static uint32_t frame_count = 0;
        if (++frame_count % 50 == 0) {
            uint32_t epoch = (uint32_t)(esp_timer_get_time() / 1000000ULL);
            ss_mesh_hub_time_sync(&g_mesh, epoch);
        }

        /* Broadcast flood status every frame when storm mode active */
        if (g_storm_mode) {
            ss_message_t status;
            ss_build_flood_status(&status, SS_HUB_NODE_ID, g_mesh.msg_seq++,
                                  g_flood_risk_level, g_flood_score,
                                  0, 0);
            uint8_t buf[SS_MAX_MSG];
            size_t len = ss_encode(&status, buf, sizeof(buf));
            ss_radio_tx(buf, (uint8_t)len);
        }
    }
}

/* === MQTT Task: Cloud Bridge === */
static void mqtt_task(void *arg)
{
    ss_message_t msg;
    while (1) {
        if (xQueueReceive(g_telemetry_queue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            mqtt_publish_telemetry(&msg);
        }

        /* Check for commands from cloud */
        ss_message_t cmd;
        if (xQueueReceive(g_command_queue, &cmd, 0) == pdTRUE) {
            xSemaphoreTake(g_radio_mutex, portMAX_DELAY);
            uint8_t buf[SS_MAX_MSG];
            size_t len = ss_encode(&cmd, buf, sizeof(buf));
            ss_radio_tx(buf, (uint8_t)len);
            xSemaphoreGive(g_radio_mutex);
        }
    }
}

/* === Edge Inference Task === */
static void inference_task(void *arg)
{
    /* TFLite-Micro model: PumpHealth anomaly detection
     * In production: load PumpHealth int8 model from flash,
     * run inference on sump sentinel vibration/current data.
     */
    ESP_LOGI(TAG, "Edge inference task started (PumpHealth TFLite-Micro)");
    while (1) {
        /* Wait for sump telemetry that needs edge analysis */
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* === Flood Risk Assessment Task === */
static void flood_risk_task(void *arg)
{
    /* In production: integrate cloud model results + local data
     * to compute StormSync Score and set storm mode.
     * When score > STORM_SCORE_THRESHOLD:
     *   - Set g_storm_mode = 1
     *   - Command sump sentinel to increase sampling rate
     *   - Command actuator to pre-close backflow valve
     *   - Activate cellular backup
     */
    ESP_LOGI(TAG, "Flood risk assessment task started");
    while (1) {
        /* Placeholder: in production, cloud sends risk updates via MQTT */
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* === OTA Distribution Task === */
static void ota_task(void *arg)
{
    ESP_LOGI(TAG, "OTA distribution task started");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

/* === LED/Status Task === */
static void status_task(void *arg)
{
    uint8_t state = 0;
    while (1) {
        state = !state;
        if (g_storm_mode) {
            set_led(state ? 100 : 0, 0, 0); /* Red blink in storm mode */
        } else {
            set_led(0, state ? 100 : 0, 0); /* Green blink normal */
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "StormSync Hub starting...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Initialize GPIOs */
    gpio_set_direction(HUB_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(HUB_GPIO_SX_RST, 1);
    gpio_set_direction(HUB_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(HUB_GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_BUZZER, GPIO_MODE_OUTPUT);

    /* Initialize I²C */
    i2c_init();

    /* Read ambient sensors */
    float temp, hum, pres;
    read_bme280(&temp, &hum, &pres);
    ESP_LOGI(TAG, "Ambient: %.1f°C, %.1f%% RH, %.0f hPa", temp, hum, pres);

    /* Initialize cellular backup */
    cellular_init();

    /* Create queues */
    g_telemetry_queue = xQueueCreate(32, sizeof(ss_message_t));
    g_command_queue = xQueueCreate(16, sizeof(ss_message_t));
    g_radio_mutex = xSemaphoreCreateMutex();

    /* Create tasks */
    xTaskCreate(mesh_task, "mesh", 8192, NULL, 5, NULL);
    xTaskCreate(mqtt_task, "mqtt", 6144, NULL, 4, NULL);
    xTaskCreate(inference_task, "inference", 6144, NULL, 3, NULL);
    xTaskCreate(flood_risk_task, "flood_risk", 4096, NULL, 4, NULL);
    xTaskCreate(ota_task, "ota", 4096, NULL, 2, NULL);
    xTaskCreate(status_task, "status", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "StormSync Hub running. Free heap: %lu bytes",
             (unsigned long)esp_get_free_heap_size());
}