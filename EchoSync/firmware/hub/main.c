/*
 * EchoSync — Hub / Gateway Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Hub coordinates the Sub-GHz mesh network, bridges to the cloud
 * via Wi-Fi/MQTT, runs local edge sound event priority classification,
 * drives the RGB LED matrix display, triggers bed-shaker relay for
 * sleeping alerts, and manages OTA firmware distribution.
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

static const char *TAG = "EchoSync-Hub";

/* === Global state === */
static es_mesh_ctx_t g_mesh;
static QueueHandle_t g_telemetry_queue;
static QueueHandle_t g_command_queue;
static SemaphoreHandle_t g_radio_mutex;
static uint8_t g_node_table[ES_SLOT_COUNT];

/* Current sound event state */
static uint8_t g_last_sound_class = 0xFF;
static uint8_t g_last_priority = 0;
static uint16_t g_last_event_id = 0;
static uint16_t g_event_counter = 0;
static uint8_t g_emergency_active = 0;

/* Sound event stats */
static uint16_t g_events_24h = 0;
static uint8_t g_emergencies_24h = 0;
static uint8_t g_room_humidity = 50;

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

static const es_spi_interface_t g_spi_iface = {
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

/* === LED Matrix Display (8×8 WS2812B) === */
static void set_led_matrix_color(uint8_t r, uint8_t g, uint8_t b)
{
    /* WS2812B via RMT peripheral in production */
    (void)r; (void)g; (void)b;
}

static void display_sound_event(uint8_t sound_class, uint8_t priority)
{
    if (priority == ES_PRIORITY_EMERGENCY) {
        set_led_matrix_color(255, 0, 0);   /* Red */
    } else if (priority == ES_PRIORITY_IMPORTANT) {
        set_led_matrix_color(255, 200, 0);  /* Yellow */
    } else {
        set_led_matrix_color(0, 100, 255);  /* Blue */
    }
}

/* === Bed Shaker Control === */
static void bed_shaker_control(uint8_t on)
{
    gpio_set_level(HUB_GPIO_BED_SHAKER, on ? 1 : 0);
    ESP_LOGI(TAG, "Bed shaker %s", on ? "ON" : "OFF");
}

/* === Buzzer Control === */
static void buzzer_control(uint8_t on)
{
    if (on) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 128);
    } else {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    }
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/* === E-ink Display (simplified) === */
static void eink_update(uint8_t sound_class, uint8_t priority,
                         uint16_t direction, uint8_t source_node)
{
    ESP_LOGI(TAG, "E-ink: class=%d priority=%d dir=%.1f° node=%d",
             sound_class, priority, direction / 10.0, source_node);
    /* In production: drive UC8151D controller via SPI */
}

/* === MQTT Client (simplified) === */
static void mqtt_publish_telemetry(const es_message_t *msg)
{
    char json[512];
    uint8_t node_id = msg->header.src;
    uint8_t subtype = msg->payload[0];

    if (subtype == ES_TELEM_SENTINEL) {
        uint8_t batt = msg->payload[1];
        uint8_t sound_class = msg->payload[2];
        uint8_t conf = msg->payload[3];
        uint16_t dir = msg->payload[4] | (msg->payload[5] << 8);
        int8_t elev = (int8_t)msg->payload[6];
        uint16_t dur = msg->payload[7] | (msg->payload[8] << 8);
        int16_t temp = msg->payload[9] | (msg->payload[10] << 8);
        uint16_t hum = msg->payload[11] | (msg->payload[12] << 8);
        uint8_t db_spl = msg->payload[13];
        uint8_t priority = msg->payload[14];
        uint16_t eid = msg->payload[15] | (msg->payload[16] << 8);

        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"sentinel\",\"battery\":%.2f,"
            "\"sound_class\":%d,\"confidence\":%d,\"direction\":%.1f,"
            "\"elevation\":%d,\"duration_ms\":%d,\"temp_c\":%.1f,"
            "\"humidity_pct\":%.1f,\"db_spl\":%d,\"priority\":%d,"
            "\"event_id\":%d}",
            node_id, batt/100.0, sound_class, conf,
            dir/10.0, elev, dur, temp/10.0, hum/10.0,
            db_spl, priority, eid);
    } else if (subtype == ES_TELEM_WRIST) {
        uint8_t batt = msg->payload[1];
        uint8_t worn = msg->payload[2];
        uint8_t sleeping = msg->payload[3];
        uint8_t last_class = msg->payload[4];
        uint8_t last_pri = msg->payload[5];
        uint16_t alerts = msg->payload[6] | (msg->payload[7] << 8);
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"wrist\",\"battery\":%.2f,"
            "\"worn\":%d,\"sleeping\":%d,\"last_alert_class\":%d,"
            "\"last_alert_priority\":%d,\"alerts_24h\":%d}",
            node_id, batt/100.0, worn, sleeping, last_class, last_pri, alerts);
    } else if (subtype == ES_TELEM_DOOR) {
        uint8_t batt = msg->payload[1];
        uint8_t evt = msg->payload[2];
        uint8_t conf = msg->payload[3];
        uint8_t knocks = msg->payload[4];
        uint16_t eid = msg->payload[5] | (msg->payload[6] << 8);
        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"door\",\"battery\":%.2f,"
            "\"event_type\":%d,\"confidence\":%d,\"knock_count\":%d,"
            "\"event_id\":%d}",
            node_id, batt/100.0, evt, conf, knocks, eid);
    } else {
        snprintf(json, sizeof(json), "{\"node\":%d,\"type\":\"unknown\"}", node_id);
    }

    ESP_LOGI(TAG, "MQTT pub: %s", json);
    /* esp_mqtt_client_publish(mqtt_client, topic, json, 0, 1, 0); */
}

/* === Local Sound Event Priority Heuristic ===
 * Combines sound class, confidence, context, and time-of-day
 * to produce priority classification without cloud.
 */
static uint8_t compute_priority(uint8_t sound_class, uint8_t confidence)
{
    /* Emergency classes always emergency */
    if (IS_EMERGENCY_CLASS(sound_class) && confidence >= ES_DETECTION_CONFIDENCE_MIN)
        return ES_PRIORITY_EMERGENCY;

    /* Important classes */
    if (IS_IMPORTANT_CLASS(sound_class) && confidence >= ES_DETECTION_CONFIDENCE_MIN)
        return ES_PRIORITY_IMPORTANT;

    return ES_PRIORITY_INFO;
}

/* === Process Sound Event === */
static void process_sound_event(uint8_t node_id, uint8_t sound_class,
                                  uint8_t confidence, uint16_t direction,
                                  int8_t elev, uint16_t duration, uint8_t priority)
{
    g_event_counter++;
    g_events_24h++;

    ESP_LOGW(TAG, "SOUND EVENT: node=%d class=%d conf=%d%% dir=%.1f° elev=%d "
             "dur=%dms priority=%d event_id=%d",
             node_id, sound_class, confidence, direction/10.0, elev,
             duration, priority, g_event_counter);

    g_last_sound_class = sound_class;
    g_last_priority = priority;
    g_last_event_id = g_event_counter;

    /* Update visual display */
    display_sound_event(sound_class, priority);
    eink_update(sound_class, priority, direction, node_id);

    /* Emergency: trigger all alert channels */
    if (priority == ES_PRIORITY_EMERGENCY) {
        g_emergency_active = 1;
        g_emergencies_24h++;
        bed_shaker_control(1);
        buzzer_control(1);

        /* Broadcast sound event to wrist band(s) */
        es_message_t event;
        es_build_sound_event(&event, ES_HUB_NODE_ID, g_mesh.msg_seq++,
                             sound_class, priority, confidence,
                             direction, node_id, 0 /* room_hash */,
                             g_event_counter, 73 /* haptic pattern */);
        uint8_t buf[ES_MAX_MSG];
        size_t len = es_encode(&event, buf, sizeof(buf));
        es_radio_tx(buf, (uint8_t)len);
    } else {
        /* Broadcast sound event to wrist band(s) via BLE */
        uint8_t haptic = (priority == ES_PRIORITY_IMPORTANT) ? 47 : 12;
        es_message_t event;
        es_build_sound_event(&event, ES_HUB_NODE_ID, g_mesh.msg_seq++,
                             sound_class, priority, confidence,
                             direction, node_id, 0, g_event_counter, haptic);
        /* In production: send via BLE to wrist band */
        ESP_LOGI(TAG, "BLE→Wrist: class=%d priority=%d haptic=%d",
                 sound_class, priority, haptic);
    }
}

/* === Mesh Coordinator Task === */
static void mesh_task(void *arg)
{
    es_radio_config_t radio_cfg = {
        .frequency = ES_NET_FREQ_HZ,
        .bandwidth = ES_NET_BW_HZ,
        .spreading_factor = ES_NET_SF,
        .coding_rate = ES_NET_CR,
        .preamble_len = ES_NET_PREAMBLE,
        .tx_power_dbm = ES_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    if (es_mesh_init(&g_mesh, ES_NODE_HUB, &g_spi_iface, &radio_cfg) != 0) {
        ESP_LOGE(TAG, "Mesh init failed");
        vTaskDelete(NULL);
    }

    g_mesh.node_id = ES_HUB_NODE_ID;
    g_mesh.tdma_slot = 0;
    g_mesh.joined = 1;

    ESP_LOGI(TAG, "Hub mesh coordinator started (node_id=0, slot=0)");

    es_message_t msg;
    while (1) {
        if (es_mesh_recv(&g_mesh, &msg, 2000) == 0) {
            switch (msg.header.type) {
                case ES_MSG_JOIN_REQ: {
                    uint8_t new_id, new_slot;
                    if (es_mesh_hub_assign_slot(&g_mesh, msg.payload[0],
                                                &new_id, &new_slot) == 0) {
                        es_message_t ack;
                        memset(&ack, 0, sizeof(ack));
                        ack.header.sync[0] = ES_SYNC0;
                        ack.header.sync[1] = ES_SYNC1;
                        ack.header.src = ES_HUB_NODE_ID;
                        ack.header.dst = new_id;
                        ack.header.type = ES_MSG_JOIN_ACK;
                        ack.header.msg_id = g_mesh.msg_seq++;
                        ack.payload[0] = new_id;
                        ack.payload[1] = new_slot;
                        ack.payload_len = 2;

                        uint8_t buf[ES_MAX_MSG];
                        size_t len = es_encode(&ack, buf, sizeof(buf));
                        es_radio_tx(buf, (uint8_t)len);

                        g_node_table[new_id] = msg.payload[0];
                        ESP_LOGI(TAG, "Node joined: id=%d slot=%d type=%d",
                                 new_id, new_slot, msg.payload[0]);
                    }
                    break;
                }
                case ES_MSG_TELEMETRY:
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    if (msg.payload[0] == ES_TELEM_SENTINEL) {
                        uint8_t sound_class = msg.payload[2];
                        uint8_t conf = msg.payload[3];
                        uint16_t dir = msg.payload[4] | (msg.payload[5] << 8);
                        int8_t elev = (int8_t)msg.payload[6];
                        uint16_t dur = msg.payload[7] | (msg.payload[8] << 8);
                        uint8_t priority = msg.payload[14];
                        uint8_t node_id = msg.header.src;
                        process_sound_event(node_id, sound_class, conf,
                                            dir, elev, dur, priority);
                    }
                    break;

                case ES_MSG_ALERT: {
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    uint8_t alert_type = msg.payload[0];
                    uint8_t severity = msg.payload[1];
                    ESP_LOGW(TAG, "ALERT from node %d: type=%d sev=%d",
                             msg.header.src, alert_type, severity);
                    break;
                }

                case ES_MSG_HEARTBEAT:
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
            es_mesh_hub_time_sync(&g_mesh, epoch);
        }
    }
}

/* === MQTT Task: Cloud Bridge === */
static void mqtt_task(void *arg)
{
    es_message_t msg;
    while (1) {
        if (xQueueReceive(g_telemetry_queue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            mqtt_publish_telemetry(&msg);
        }

        /* Check for commands from cloud */
        es_message_t cmd;
        if (xQueueReceive(g_command_queue, &cmd, 0) == pdTRUE) {
            xSemaphoreTake(g_radio_mutex, portMAX_DELAY);
            uint8_t buf[ES_MAX_MSG];
            size_t len = es_encode(&cmd, buf, sizeof(buf));
            es_radio_tx(buf, (uint8_t)len);
            xSemaphoreGive(g_radio_mutex);
        }
    }
}

/* === Display & Alert Task === */
static void display_task(void *arg)
{
    ESP_LOGI(TAG, "Display task started");
    while (1) {
        /* Reset daily counters at midnight (simplified) */
        static uint32_t last_reset = 0;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000000ULL);
        if (now - last_reset >= 86400) {
            g_events_24h = 0;
            g_emergencies_24h = 0;
            last_reset = now;
        }

        /* Turn off emergency indicators after timeout */
        if (g_emergency_active) {
            static uint32_t emergency_start = 0;
            if (emergency_start == 0)
                emergency_start = now;
            if (now - emergency_start >= HUB_BED_SHAKER_DURATION_MS / 1000) {
                bed_shaker_control(0);
                buzzer_control(0);
                g_emergency_active = 0;
                emergency_start = 0;
            }
        }

        ESP_LOGI(TAG, "Events: 24h=%d emerg=%d last_class=%d last_pri=%d",
                 g_events_24h, g_emergencies_24h, g_last_sound_class,
                 g_last_priority);

        vTaskDelay(pdMS_TO_TICKS(HUB_DISPLAY_REFRESH_MS));
    }
}

/* === Ambient Monitor Task === */
static void ambient_task(void *arg)
{
    float temp, hum, pres;
    while (1) {
        read_bme280(&temp, &hum, &pres);
        g_room_humidity = (uint8_t)hum;
        ESP_LOGI(TAG, "Ambient: T=%.1f°C H=%.1f%% P=%.0fhPa",
                 temp, hum, pres);
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

/* === GPIO Setup === */
static void gpio_setup(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << HUB_GPIO_BED_SHAKER) |
                        (1ULL << HUB_GPIO_BUZZER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(HUB_GPIO_BED_SHAKER, 0);

    /* Buzzer PWM via LEDC */
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 3000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t chan_conf = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = HUB_GPIO_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .hpoint = 0,
    };
    ledc_channel_config(&chan_conf);
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "EchoSync Hub starting...");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
        nvs_flash_erase();

    gpio_setup();
    i2c_init();

    g_telemetry_queue = xQueueCreate(32, sizeof(es_message_t));
    g_command_queue = xQueueCreate(16, sizeof(es_message_t));
    g_radio_mutex = xSemaphoreCreateMutex();

    xTaskCreate(mesh_task, "mesh_task", 8192, NULL, 5, NULL);
    xTaskCreate(mqtt_task, "mqtt_task", 6144, NULL, 4, NULL);
    xTaskCreate(display_task, "display_task", 4096, NULL, 3, NULL);
    xTaskCreate(ambient_task, "ambient_task", 4096, NULL, 2, NULL);

    ESP_LOGI(TAG, "EchoSync Hub ready");
}