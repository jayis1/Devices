/*
 * VoiceSync — Hub / Gateway Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Hub coordinates the Sub-GHz mesh network, bridges to the cloud
 * via Wi-Fi/MQTT, runs local edge vocal health assessment, broadcasts
 * voice status, controls the smart humidifier, and manages OTA
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

static const char *TAG = "VoiceSync-Hub";

/* === Global state === */
static vs_mesh_ctx_t g_mesh;
static QueueHandle_t g_telemetry_queue;
static QueueHandle_t g_command_queue;
static SemaphoreHandle_t g_radio_mutex;
static uint8_t g_node_table[VS_SLOT_COUNT];

/* Current vocal health (broadcast to nodes) */
static uint8_t g_vocal_health_score = 100;
static uint8_t g_disorder_risk_score = 0;
static uint8_t g_risk_level = 0;  /* 0=low, 1=moderate, 2=high, 3=critical */
static uint8_t g_high_risk_mode = 0;
static uint8_t g_rest_recommended = 0;
static uint16_t g_rest_minutes_remaining = 0;

/* Counters for local risk heuristic */
static uint8_t  g_phonation_pct_today = 0;
static uint8_t  g_hydration_pct = 100;
static uint16_t g_vocal_load_1h = 0;
static uint8_t  g_hoarseness_count_24h = 0;
static uint8_t  g_reflux_detected = 0;
static uint8_t  g_critical_voice_class = 0;
static float    g_room_humidity = 50.0;

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

static const vs_spi_interface_t g_spi_iface = {
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

/* === Smart Humidifier Control === */
static void humidifier_control(uint8_t on)
{
    gpio_set_level(HUB_GPIO_HUM_RELAY, on ? 1 : 0);
    ESP_LOGI(TAG, "Humidifier %s", on ? "ON" : "OFF");
}

/* === MQTT Client (simplified) === */
static void mqtt_publish_telemetry(const vs_message_t *msg)
{
    char json[512];
    uint8_t node_id = msg->header.src;
    uint8_t subtype = msg->payload[0];

    if (subtype == VS_TELEM_VOCAL_BAND) {
        uint8_t batt = msg->payload[1];
        uint16_t f0 = msg->payload[2] | (msg->payload[3] << 8);
        uint16_t jitter = msg->payload[4] | (msg->payload[5] << 8);
        uint16_t shimmer = msg->payload[6] | (msg->payload[7] << 8);
        int8_t hnr = (int8_t)msg->payload[8];
        uint8_t phonation = msg->payload[9];
        uint8_t intensity = msg->payload[10];
        uint16_t skin_temp = msg->payload[15] | (msg->payload[16] << 8);
        uint8_t hr = msg->payload[17];
        uint8_t hrv = msg->payload[18];
        uint8_t stress = msg->payload[19];
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"vocal_band\",\"battery\":%.2f,"
            "\"f0_hz\":%.1f,\"jitter_pct\":%.2f,\"shimmer_pct\":%.2f,"
            "\"hnr_db\":%d,\"phonation_pct\":%d,\"intensity_db\":%d,"
            "\"skin_temp_c\":%.2f,\"heart_rate\":%d,\"hrv_rmssd\":%d,"
            "\"stress_level\":%d}",
            node_id, batt/100.0, f0/10.0, jitter/100.0, shimmer/100.0,
            hnr, phonation, intensity, (skin_temp+2000)/100.0,
            hr, hrv, stress);
    } else if (subtype == VS_TELEM_ROOM) {
        uint8_t vq_class = msg->payload[2];
        uint8_t conf = msg->payload[3];
        uint16_t f0 = msg->payload[4] | (msg->payload[5] << 8);
        uint8_t phonation = msg->payload[6];
        int16_t temp = msg->payload[7] | (msg->payload[8] << 8);
        uint16_t hum = msg->payload[9] | (msg->payload[10] << 8);
        uint16_t voc = msg->payload[11] | (msg->payload[12] << 8);
        uint8_t db_spl = msg->payload[13];
        uint8_t talking = msg->payload[14];
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"room\",\"voice_quality\":%d,"
            "\"confidence\":%d,\"f0_hz\":%.1f,\"phonation_pct\":%d,"
            "\"temp_c\":%.1f,\"humidity_pct\":%.1f,\"voc_index\":%d,"
            "\"db_spl\":%d,\"talking\":%d}",
            node_id, vq_class, conf, f0/10.0, phonation,
            temp/10.0, hum/10.0, voc, db_spl, talking);
    } else if (subtype == VS_TELEM_HYDRATION) {
        uint8_t batt = msg->payload[1];
        uint16_t mass = msg->payload[2] | (msg->payload[3] << 8);
        uint16_t sips = msg->payload[4] | (msg->payload[5] << 8);
        uint16_t intake = msg->payload[6] | (msg->payload[7] << 8);
        uint8_t last_sip = msg->payload[8];
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"hydration\",\"battery\":%.2f,"
            "\"water_mass_g\":%d,\"sips_24h\":%d,\"intake_ml\":%d,"
            "\"last_sip_min\":%d}",
            node_id, batt/100.0, mass, sips, intake, last_sip);
    } else if (subtype == VS_TELEM_HUMIDITY) {
        int16_t temp = msg->payload[2] | (msg->payload[3] << 8);
        uint16_t hum = msg->payload[4] | (msg->payload[5] << 8);
        uint8_t tank = msg->payload[6];
        uint8_t hum_on = msg->payload[7];
        uint8_t fan_on = msg->payload[8];
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"humidity\",\"temp_c\":%.1f,"
            "\"humidity_pct\":%.1f,\"tank_pct\":%d,"
            "\"humidifier_on\":%d,\"fan_on\":%d}",
            node_id, temp/10.0, hum/10.0, tank, hum_on, fan_on);
    } else {
        snprintf(json, sizeof(json), "{\"node\":%d,\"type\":\"unknown\"}", node_id);
    }

    ESP_LOGI(TAG, "MQTT pub: %s", json);
    /* esp_mqtt_client_publish(mqtt_client, topic, json, 0, 1, 0); */
}

/* === Local Vocal Health Heuristic ===
 * Combines phonation %, hydration, voice quality alerts, and environmental
 * factors to produce a quick local vocal health estimate without cloud.
 */
static void compute_local_vocal_health(void)
{
    uint8_t base = 100;

    /* Phonation % above NCVS safe dose reduces score */
    if (g_phonation_pct_today > PHONATION_SAFE_PCT) {
        uint8_t excess = g_phonation_pct_today - PHONATION_SAFE_PCT;
        base -= (excess > 40) ? 30 : (excess > 20) ? 20 : 10;
    }

    /* Hoarseness detections reduce score */
    if (g_hoarseness_count_24h > 0) base -= 10;
    if (g_hoarseness_count_24h > 3) base -= 15;
    if (g_hoarseness_count_24h > 10) base -= 15;

    /* Reflux pattern reduces score significantly */
    if (g_reflux_detected) base -= 20;

    /* Critical voice class detected */
    if (g_critical_voice_class) base -= 25;

    /* Hydration below target reduces score */
    if (g_hydration_pct < HYDRATION_LOW_PCT) base -= 15;

    /* Low humidity reduces score */
    if (g_room_humidity < HUMIDITY_TARGET_MIN) base -= 10;

    g_vocal_health_score = base > 100 ? 100 : base;

    /* Disorder risk is inverse of health */
    g_disorder_risk_score = 100 - g_vocal_health_score;

    /* Risk level */
    if (g_disorder_risk_score >= VOICE_RISK_CRITICAL || g_critical_voice_class) {
        g_risk_level = 3; /* Critical */
    } else if (g_disorder_risk_score >= VOICE_RISK_HIGH || g_hoarseness_count_24h > 5) {
        g_risk_level = 2; /* High */
    } else if (g_disorder_risk_score >= 25 || g_hoarseness_count_24h > 0) {
        g_risk_level = 1; /* Moderate */
    } else {
        g_risk_level = 0; /* Low */
    }

    /* High-risk mode */
    g_high_risk_mode = (g_risk_level >= 2) ? 1 : 0;

    /* Rest recommendation */
    if (g_phonation_pct_today > PHONATION_SAFE_PCT + 10) {
        g_rest_recommended = 1;
        g_rest_minutes_remaining = VOCAL_REST_MIN;
    } else {
        g_rest_recommended = 0;
        g_rest_minutes_remaining = 0;
    }

    /* Auto-control humidifier based on humidity */
    if (g_room_humidity < HUMIDITY_TARGET_MIN) {
        humidifier_control(1);
    } else if (g_room_humidity > HUMIDITY_TARGET_MAX) {
        humidifier_control(0);
    }
}

/* === Broadcast voice status to all nodes === */
static void broadcast_voice_status(void)
{
    vs_message_t status;
    vs_build_voice_status(&status, VS_HUB_NODE_ID, g_mesh.msg_seq++,
                          g_risk_level, g_vocal_health_score,
                          g_disorder_risk_score, g_phonation_pct_today,
                          g_hydration_pct, g_rest_recommended,
                          g_rest_minutes_remaining);
    uint8_t buf[VS_MAX_MSG];
    size_t len = vs_encode(&status, buf, sizeof(buf));
    vs_radio_tx(buf, (uint8_t)len);
}

/* === Mesh Coordinator Task === */
static void mesh_task(void *arg)
{
    vs_radio_config_t radio_cfg = {
        .frequency = VS_NET_FREQ_HZ,
        .bandwidth = VS_NET_BW_HZ,
        .spreading_factor = VS_NET_SF,
        .coding_rate = VS_NET_CR,
        .preamble_len = VS_NET_PREAMBLE,
        .tx_power_dbm = VS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    if (vs_mesh_init(&g_mesh, VS_NODE_HUB, &g_spi_iface, &radio_cfg) != 0) {
        ESP_LOGE(TAG, "Mesh init failed");
        vTaskDelete(NULL);
    }

    g_mesh.node_id = VS_HUB_NODE_ID;
    g_mesh.tdma_slot = 0;
    g_mesh.joined = 1;

    ESP_LOGI(TAG, "Hub mesh coordinator started (node_id=0, slot=0)");

    vs_message_t msg;
    while (1) {
        if (vs_mesh_recv(&g_mesh, &msg, 2000) == 0) {
            switch (msg.header.type) {
                case VS_MSG_JOIN_REQ: {
                    uint8_t new_id, new_slot;
                    if (vs_mesh_hub_assign_slot(&g_mesh, msg.payload[0],
                                                &new_id, &new_slot) == 0) {
                        vs_message_t ack;
                        memset(&ack, 0, sizeof(ack));
                        ack.header.sync[0] = VS_SYNC0;
                        ack.header.sync[1] = VS_SYNC1;
                        ack.header.src = VS_HUB_NODE_ID;
                        ack.header.dst = new_id;
                        ack.header.type = VS_MSG_JOIN_ACK;
                        ack.header.msg_id = g_mesh.msg_seq++;
                        ack.payload[0] = new_id;
                        ack.payload[1] = new_slot;
                        ack.payload_len = 2;

                        uint8_t buf[VS_MAX_MSG];
                        size_t len = vs_encode(&ack, buf, sizeof(buf));
                        vs_radio_tx(buf, (uint8_t)len);

                        g_node_table[new_id] = msg.payload[0];
                        ESP_LOGI(TAG, "Node joined: id=%d slot=%d type=%d",
                                 new_id, new_slot, msg.payload[0]);
                    }
                    break;
                }
                case VS_MSG_TELEMETRY:
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    /* Track voice quality for local risk */
                    if (msg.payload[0] == VS_TELEM_ROOM) {
                        uint8_t vq_class = msg.payload[2];
                        if (IS_VOICE_DISORDER_CLASS(vq_class)) {
                            if (vq_class == VS_VOICE_HOARSE)
                                g_hoarseness_count_24h++;
                            if (vq_class == VS_VOICE_REFLUX)
                                g_reflux_detected = 1;
                            if (IS_VOICE_CRITICAL_CLASS(vq_class))
                                g_critical_voice_class = 1;
                        }
                        /* Track room humidity */
                        uint16_t hum = msg.payload[9] | (msg.payload[10] << 8);
                        g_room_humidity = hum / 10.0f;
                    }
                    if (msg.payload[0] == VS_TELEM_VOCAL_BAND) {
                        g_phonation_pct_today = msg.payload[9];
                    }
                    if (msg.payload[0] == VS_TELEM_HYDRATION) {
                        uint16_t intake = msg.payload[6] | (msg.payload[7] << 8);
                        g_hydration_pct = (uint8_t)(intake * 100 / HYDRATION_TARGET_ML);
                    }
                    break;

                case VS_MSG_VOICE_ALERT: {
                    uint8_t vq_class = msg.payload[0];
                    uint8_t confidence = msg.payload[1];
                    uint16_t f0 = msg.payload[2] | (msg.payload[3] << 8);
                    uint8_t is_critical = msg.payload[4];
                    ESP_LOGW(TAG, "VOICE ALERT from node %d: class=%d "
                             "conf=%d%% f0=%.1fHz critical=%d",
                             msg.header.src, vq_class, confidence,
                             f0/10.0, is_critical);
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    if (is_critical) {
                        g_critical_voice_class = 1;
                        broadcast_voice_status();
                    }
                    break;
                }

                case VS_MSG_ALERT: {
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    uint8_t alert_type = msg.payload[0];
                    uint8_t severity = msg.payload[1];
                    ESP_LOGW(TAG, "ALERT from node %d: type=%d sev=%d",
                             msg.header.src, alert_type, severity);
                    break;
                }

                case VS_MSG_HEARTBEAT:
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
            vs_mesh_hub_time_sync(&g_mesh, epoch);
        }

        /* Broadcast voice status when high-risk mode active */
        if (g_high_risk_mode) {
            broadcast_voice_status();
        }
    }
}

/* === MQTT Task: Cloud Bridge === */
static void mqtt_task(void *arg)
{
    vs_message_t msg;
    while (1) {
        if (xQueueReceive(g_telemetry_queue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            mqtt_publish_telemetry(&msg);
        }

        /* Check for commands from cloud */
        vs_message_t cmd;
        if (xQueueReceive(g_command_queue, &cmd, 0) == pdTRUE) {
            xSemaphoreTake(g_radio_mutex, portMAX_DELAY);
            uint8_t buf[VS_MAX_MSG];
            size_t len = vs_encode(&cmd, buf, sizeof(buf));
            vs_radio_tx(buf, (uint8_t)len);
            xSemaphoreGive(g_radio_mutex);
        }
    }
}

/* === Edge Vocal Health Assessment Task === */
static void health_task(void *arg)
{
    ESP_LOGI(TAG, "Edge vocal health assessment task started");
    while (1) {
        /* Reset daily counters at midnight (simplified) */
        static uint32_t last_reset = 0;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000000ULL);
        if (now - last_reset >= 86400) {
            g_hoarseness_count_24h = 0;
            g_reflux_detected = 0;
            g_critical_voice_class = 0;
            last_reset = now;
        }

        compute_local_vocal_health();

        ESP_LOGI(TAG, "Vocal Health: score=%d risk=%d level=%d phonation=%d%% "
                 "hydration=%d%% humidity=%.1f%%",
                 g_vocal_health_score, g_disorder_risk_score,
                 g_risk_level, g_phonation_pct_today,
                 g_hydration_pct, g_room_humidity);

        /* Broadcast status every 30 seconds */
        broadcast_voice_status();

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

/* === Ambient Monitor Task === */
static void ambient_task(void *arg)
{
    float temp, hum, pres;
    while (1) {
        read_bme280(&temp, &hum, &pres);
        ESP_LOGI(TAG, "Ambient: %.1fC, %.1f%% RH, %.0f hPa", temp, hum, pres);

        /* Use hub BME280 as backup humidity reading */
        if (g_room_humidity < 1.0) {
            g_room_humidity = hum;
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
    ESP_LOGI(TAG, "VoiceSync Hub starting...");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Initialize GPIO for humidifier relay */
    gpio_set_direction(HUB_GPIO_HUM_RELAY, GPIO_MODE_OUTPUT);
    gpio_set_level(HUB_GPIO_HUM_RELAY, 0);

    /* Initialize I2C */
    i2c_init();

    /* Create queues and mutex */
    g_telemetry_queue = xQueueCreate(32, sizeof(vs_message_t));
    g_command_queue = xQueueCreate(8, sizeof(vs_message_t));
    g_radio_mutex = xSemaphoreCreateMutex();

    /* Initialize SPI for SX1262 */
    spi_init();

    /* Start tasks */
    xTaskCreate(mesh_task, "mesh", 8192, NULL, 5, NULL);
    xTaskCreate(mqtt_task, "mqtt", 6144, NULL, 4, NULL);
    xTaskCreate(health_task, "health", 4096, NULL, 3, NULL);
    xTaskCreate(ambient_task, "ambient", 4096, NULL, 2, NULL);
    xTaskCreate(status_task, "status", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "VoiceSync Hub running");
}