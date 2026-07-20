/*
 * MosquitoSync — Hub / Gateway Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Hub coordinates the Sub-GHz mesh network, bridges to the cloud
 * via Wi-Fi/MQTT with 4G LTE cellular backup, runs local edge risk
 * assessment (BiteRisk heuristic), broadcasts risk status, commands
 * window barriers to close on mosquito detection, and manages OTA
 * firmware distribution.
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

static const char *TAG = "MosquitoSync-Hub";

/* === Global state === */
static ms_mesh_ctx_t g_mesh;
static QueueHandle_t g_telemetry_queue;
static QueueHandle_t g_command_queue;
static SemaphoreHandle_t g_radio_mutex;
static uint8_t g_node_table[MS_SLOT_COUNT];

/* Current risk (broadcast to nodes) */
static uint8_t g_bite_risk_score = 0;
static uint8_t g_disease_risk_score = 0;
static uint8_t g_activity_index = 0;
static uint8_t g_risk_level = 0;  /* 0=low, 1=moderate, 2=high, 3=critical */
static uint8_t g_high_risk_mode = 0;

/* Counters for local risk heuristic */
static uint16_t g_acoustic_detections_1h = 0;
static uint16_t g_trap_captures_24h = 0;
static uint8_t  g_disease_vector_detected = 0;

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

static const ms_spi_interface_t g_spi_iface = {
    .init = spi_init,
    .cs_select = spi_cs_select,
    .cs_release = spi_cs_release,
    .transfer = spi_transfer,
    .reset = spi_reset,
    .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read,
    .dio1_irq_enable = spi_dio1_irq_enable,
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
    /* Initialize UART2 for SIM7000 4G LTE module */
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
    /* In production: send SMS or MQTT over 4G LTE */
    ESP_LOGI(TAG, "CELLULAR ALERT: %s", message);
    return 0;
}

/* === MQTT Client (simplified) === */
static void mqtt_publish_telemetry(const ms_message_t *msg)
{
    char json[512];
    uint8_t node_id = msg->header.src;
    uint8_t subtype = msg->payload[0];

    if (subtype == MS_TELEM_ACOUSTIC) {
        uint8_t detected = msg->payload[4];
        uint8_t species = msg->payload[5];
        uint8_t conf = msg->payload[6];
        uint16_t freq = msg->payload[7] | (msg->payload[8] << 8);
        uint16_t det24 = msg->payload[9] | (msg->payload[10] << 8);
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"acoustic\",\"detected\":%d,"
            "\"species\":%d,\"confidence\":%d,\"freq_hz\":%.1f,"
            "\"detections_24h\":%d}",
            node_id, detected, species, conf, freq/10.0, det24);
    } else if (subtype == MS_TELEM_TRAP) {
        int16_t temp = msg->payload[2] | (msg->payload[3] << 8);
        uint16_t ir_brk = msg->payload[10] | (msg->payload[11] << 8);
        uint16_t cap24 = msg->payload[12] | (msg->payload[13] << 8);
        uint8_t co2_on = msg->payload[15];
        uint8_t propane = msg->payload[16];
        uint8_t species = msg->payload[18];
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"trap\",\"temp\":%.1f,"
            "\"ir_breaks\":%d,\"capture_24h\":%d,\"co2_on\":%d,"
            "\"propane_pct\":%d,\"species\":%d}",
            node_id, temp/10.0, ir_brk, cap24, co2_on, propane, species);
    } else if (subtype == MS_TELEM_BARRIER) {
        uint8_t status = msg->payload[2];
        uint8_t trigger = msg->payload[3];
        uint8_t cycles = msg->payload[4];
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"barrier\",\"status\":%d,"
            "\"trigger\":%d,\"cycles_24h\":%d}",
            node_id, status, trigger, cycles);
    } else if (subtype == MS_TELEM_WEATHER) {
        int16_t temp = msg->payload[2] | (msg->payload[3] << 8);
        uint16_t hum = msg->payload[4] | (msg->payload[5] << 8);
        uint16_t pres = msg->payload[6] | (msg->payload[7] << 8);
        uint16_t wind = msg->payload[8] | (msg->payload[9] << 8);
        uint16_t rain = msg->payload[12] | (msg->payload[13] << 8);
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"weather\",\"temp\":%.1f,"
            "\"humidity\":%.1f,\"pressure\":%.1f,\"wind\":%.1f,"
            "\"rain_mm\":%.1f}",
            node_id, temp/10.0, hum/10.0, (pres+8000)/10.0,
            wind/10.0, rain*0.2);
    } else {
        snprintf(json, sizeof(json), "{\"node\":%d,\"type\":\"unknown\"}", node_id);
    }

    ESP_LOGI(TAG, "MQTT pub: %s", json);
    /* esp_mqtt_client_publish(mqtt_client, topic, json, 0, 1, 0); */
}

/* === Local BiteRisk Heuristic ===
 * Combines acoustic detections, trap counts, and disease vector presence
 * to produce a quick local risk estimate without waiting for cloud.
 */
static void compute_local_risk(void)
{
    uint8_t base = 0;

    /* Acoustic detections in last hour contribute 0-40 points */
    if (g_acoustic_detections_1h > 0) base += 10;
    if (g_acoustic_detections_1h > 3) base += 15;
    if (g_acoustic_detections_1h > 10) base += 15;

    /* Trap captures contribute 0-30 points */
    if (g_trap_captures_24h > 10) base += 10;
    if (g_trap_captures_24h > 50) base += 10;
    if (g_trap_captures_24h > 200) base += 10;

    /* Disease vector species present = +30 (max) */
    if (g_disease_vector_detected) base += 30;

    g_bite_risk_score = base > 100 ? 100 : base;

    /* Risk level */
    if (g_disease_risk_score >= DISEASE_RISK_CRITICAL || base >= 76) {
        g_risk_level = 3; /* Critical */
    } else if (g_disease_risk_score >= DISEASE_RISK_HIGH || base >= 51) {
        g_risk_level = 2; /* High */
    } else if (base >= 25) {
        g_risk_level = 1; /* Moderate */
    } else {
        g_risk_level = 0; /* Low */
    }

    /* High-risk mode */
    g_high_risk_mode = (g_risk_level >= 2) ? 1 : 0;
}

/* === Command all barriers to close === */
static void command_all_barriers_close(uint8_t trigger_source)
{
    ms_message_t cmd;
    for (uint8_t id = 1; id < MS_SLOT_COUNT; id++) {
        if (g_node_table[id] == MS_NODE_BARRIER) {
            uint8_t data[1] = {trigger_source};
            ms_build_command(&cmd, MS_HUB_NODE_ID, id, g_mesh.msg_seq++,
                             MS_CMD_BARRIER_CLOSE, data, 1);
            uint8_t buf[MS_MAX_MSG];
            size_t len = ms_encode(&cmd, buf, sizeof(buf));
            ms_radio_tx(buf, (uint8_t)len);
        }
    }
    ESP_LOGI(TAG, "All barriers commanded to close (trigger=%d)", trigger_source);
}

/* === Mesh Coordinator Task === */
static void mesh_task(void *arg)
{
    ms_radio_config_t radio_cfg = {
        .frequency = MS_NET_FREQ_HZ,
        .bandwidth = MS_NET_BW_HZ,
        .spreading_factor = MS_NET_SF,
        .coding_rate = MS_NET_CR,
        .preamble_len = MS_NET_PREAMBLE,
        .tx_power_dbm = MS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    if (ms_mesh_init(&g_mesh, MS_NODE_HUB, &g_spi_iface, &radio_cfg) != 0) {
        ESP_LOGE(TAG, "Mesh init failed");
        vTaskDelete(NULL);
    }

    g_mesh.node_id = MS_HUB_NODE_ID;
    g_mesh.tdma_slot = 0;
    g_mesh.joined = 1;

    ESP_LOGI(TAG, "Hub mesh coordinator started (node_id=0, slot=0)");

    ms_message_t msg;
    while (1) {
        if (ms_mesh_recv(&g_mesh, &msg, 2000) == 0) {
            switch (msg.header.type) {
                case MS_MSG_JOIN_REQ: {
                    uint8_t new_id, new_slot;
                    if (ms_mesh_hub_assign_slot(&g_mesh, msg.payload[0],
                                                &new_id, &new_slot) == 0) {
                        ms_message_t ack;
                        memset(&ack, 0, sizeof(ack));
                        ack.header.sync[0] = MS_SYNC0;
                        ack.header.sync[1] = MS_SYNC1;
                        ack.header.src = MS_HUB_NODE_ID;
                        ack.header.dst = new_id;
                        ack.header.type = MS_MSG_JOIN_ACK;
                        ack.header.msg_id = g_mesh.msg_seq++;
                        ack.payload[0] = new_id;
                        ack.payload[1] = new_slot;
                        ack.payload_len = 2;

                        uint8_t buf[MS_MAX_MSG];
                        size_t len = ms_encode(&ack, buf, sizeof(buf));
                        ms_radio_tx(buf, (uint8_t)len);

                        g_node_table[new_id] = msg.payload[0]; /* Store node type */
                        ESP_LOGI(TAG, "Node joined: id=%d slot=%d type=%d",
                                 new_id, new_slot, msg.payload[0]);
                    }
                    break;
                }
                case MS_MSG_TELEMETRY:
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    /* Track acoustic detections for local risk */
                    if (msg.payload[0] == MS_TELEM_ACOUSTIC) {
                        if (msg.payload[4]) { /* mosquito_detected */
                            g_acoustic_detections_1h++;
                            uint8_t species = msg.payload[5];
                            if (IS_DISEASE_VECTOR(species)) {
                                g_disease_vector_detected = 1;
                                ESP_LOGW(TAG, "DISEASE VECTOR detected: species=%d",
                                         species);
                                /* Immediate barrier close + cellular alert */
                                command_all_barriers_close(2); /* auto-detected */
                                cellular_send_alert(
                                    "DISEASE VECTOR mosquito detected! "
                                    "Barriers closing.");
                            }
                        }
                    }
                    break;

                case MS_MSG_SPECIES_ALERT: {
                    uint8_t species = msg.payload[0];
                    uint8_t confidence = msg.payload[1];
                    uint16_t freq = msg.payload[2] | (msg.payload[3] << 8);
                    uint8_t is_vector = msg.payload[4];
                    ESP_LOGW(TAG, "SPECIES ALERT from node %d: species=%d "
                             "conf=%d%% freq=%.1fHz vector=%d",
                             msg.header.src, species, confidence,
                             freq/10.0, is_vector);
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    if (is_vector) {
                        command_all_barriers_close(2);
                        cellular_send_alert("Disease vector species detected!");
                    }
                    break;
                }

                case MS_MSG_ALERT: {
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    uint8_t alert_type = msg.payload[0];
                    uint8_t severity = msg.payload[1];
                    ESP_LOGW(TAG, "ALERT from node %d: type=%d sev=%d",
                             msg.header.src, alert_type, severity);
                    if (severity >= 3) {
                        cellular_send_alert("CRITICAL: MosquitoSync alert!");
                    }
                    break;
                }

                case MS_MSG_HEARTBEAT:
                    ESP_LOGI(TAG, "Heartbeat from node %d, rssi=%d",
                             msg.header.src, msg.payload[1]);
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
            ms_mesh_hub_time_sync(&g_mesh, epoch);
        }

        /* Broadcast risk status every frame when high-risk mode active */
        if (g_high_risk_mode) {
            ms_message_t status;
            ms_build_risk_status(&status, MS_HUB_NODE_ID, g_mesh.msg_seq++,
                                 g_risk_level, g_bite_risk_score,
                                 g_disease_risk_score, g_activity_index, 0);
            uint8_t buf[MS_MAX_MSG];
            size_t len = ms_encode(&status, buf, sizeof(buf));
            ms_radio_tx(buf, (uint8_t)len);
        }
    }
}

/* === MQTT Task: Cloud Bridge === */
static void mqtt_task(void *arg)
{
    ms_message_t msg;
    while (1) {
        if (xQueueReceive(g_telemetry_queue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            mqtt_publish_telemetry(&msg);
        }

        /* Check for commands from cloud */
        ms_message_t cmd;
        if (xQueueReceive(g_command_queue, &cmd, 0) == pdTRUE) {
            xSemaphoreTake(g_radio_mutex, portMAX_DELAY);
            uint8_t buf[MS_MAX_MSG];
            size_t len = ms_encode(&cmd, buf, sizeof(buf));
            ms_radio_tx(buf, (uint8_t)len);
            xSemaphoreGive(g_radio_mutex);
        }
    }
}

/* === Edge Risk Assessment Task === */
static void risk_task(void *arg)
{
    ESP_LOGI(TAG, "Edge risk assessment task started");
    while (1) {
        /* Reset hourly counter */
        static uint32_t last_reset = 0;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000000ULL);
        if (now - last_reset >= 3600) {
            g_acoustic_detections_1h = 0;
            last_reset = now;
        }

        compute_local_risk();

        ESP_LOGI(TAG, "Risk: bite=%d disease=%d activity=%d level=%d mode=%d",
                 g_bite_risk_score, g_disease_risk_score,
                 g_activity_index, g_risk_level, g_high_risk_mode);

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

/* === Weather Aggregator Task ===
 * Reads local BME280 + aggregates weather data for local risk computation.
 */
static void weather_task(void *arg)
{
    float temp, hum, pres;
    while (1) {
        read_bme280(&temp, &hum, &pres);
        ESP_LOGI(TAG, "Ambient: %.1fC, %.1f%% RH, %.0f hPa", temp, hum, pres);

        /* Mosquito activity heuristic: peak at 27C, active 15-32C */
        if (temp >= 15.0 && temp <= 32.0) {
            float temp_factor = 1.0 - fabs(temp - 27.0) / 12.0;
            if (temp_factor > 0) {
                g_activity_index = (uint8_t)(temp_factor * 60); /* 0-60 from temp */
            }
        }

        vTaskDelay(pdMS_TO_TICKS(300000)); /* 5 minutes */
    }
}

/* === LED/Status Task === */
static void status_task(void *arg)
{
    uint8_t state = 0;
    while (1) {
        state = !state;
        if (g_high_risk_mode) {
            set_led(state ? 100 : 0, 0, 0); /* Red blink in high-risk mode */
        } else {
            set_led(0, state ? 100 : 0, 0); /* Green blink normal */
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "MosquitoSync Hub starting...");

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

    /* Initialize I2C */
    i2c_init();

    /* Read ambient sensors */
    float temp, hum, pres;
    read_bme280(&temp, &hum, &pres);
    ESP_LOGI(TAG, "Ambient: %.1fC, %.1f%% RH, %.0f hPa", temp, hum, pres);

    /* Initialize cellular backup */
    cellular_init();

    /* Create queues */
    g_telemetry_queue = xQueueCreate(32, sizeof(ms_message_t));
    g_command_queue = xQueueCreate(16, sizeof(ms_message_t));
    g_radio_mutex = xSemaphoreCreateMutex();

    /* Create tasks */
    xTaskCreate(mesh_task, "mesh", 8192, NULL, 5, NULL);
    xTaskCreate(mqtt_task, "mqtt", 6144, NULL, 4, NULL);
    xTaskCreate(risk_task, "risk", 4096, NULL, 4, NULL);
    xTaskCreate(weather_task, "weather", 4096, NULL, 3, NULL);
    xTaskCreate(ota_task, "ota", 4096, NULL, 2, NULL);
    xTaskCreate(status_task, "status", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "MosquitoSync Hub running. Free heap: %lu bytes",
             (unsigned long)esp_get_free_heap_size());
}