/**
 * @file main.c
 * @brief SoundNest Hub Node — Main entry point.
 *
 * The hub node is the brain of the SoundNest system. It runs on ESP32-S3
 * with 8MB PSRAM and coordinates all Sub-GHz mesh nodes, runs local
 * audio ML inference, bridges to WiFi/BLE/cloud, and drives the TFT display.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ESP-IDF headers */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/spi_master.h"

/* SoundNest headers */
#include "mesh_manager.h"
#include "audio_ml.h"
#include "mqtt_manager.h"
#include "ble_gatt.h"
#include "display.h"
#include "alarm_manager.h"
#include "dose_calculator.h"
#include "masking_engine.h"
#include "tinnitus.h"
#include "ota_manager.h"
#include "../common/protocol/mesh_packet.h"
#include "../common/dsp/spl.h"

static const char *TAG = "SOUNDNEST_HUB";

/* ── FreeRTOS Task Priorities ──────────────────────────────────────── */

#define TASK_PRIORITY_MESH       5   /* Highest: radio communication */
#define TASK_PRIORITY_AUDIO_ML   4   /* High: real-time audio processing */
#define TASK_PRIORITY_ALARM       4   /* High: safety alerts */
#define TASK_PRIORITY_MQTT        3   /* Medium: cloud communication */
#define TASK_PRIORITY_BLE         3   /* Medium: mobile app */
#define TASK_PRIORITY_DISPLAY     2   /* Low: UI rendering */
#define TASK_PRIORITY_DOSE        2   /* Low: dose calculation */
#define TASK_PRIORITY_MASKING     2   /* Low: masking synthesis */

/* ── Task Stack Sizes ──────────────────────────────────────────────── */

#define STACK_MESH          4096
#define STACK_AUDIO_ML      8192
#define STACK_MQTT          6144
#define STACK_BLE           4096
#define STACK_DISPLAY       6144
#define STACK_ALARM         2048
#define STACK_DOSE          4096
#define STACK_MASKING       4096

/* ── Queue Sizes ────────────────────────────────────────────────────── */

#define QUEUE_SIZE_EVENTS       32
#define QUEUE_SIZE_SPL          16
#define QUEUE_SIZE_ALERTS        8
#define QUEUE_SIZE_MASKING       8

/* ── Global State ──────────────────────────────────────────────────── */

typedef struct {
    /* System */
    bool initialized;
    bool wifi_connected;
    bool mqtt_connected;
    bool ble_connected;
    uint32_t uptime_sec;
    uint16_t events_today;
    uint16_t packets_sent;
    uint16_t packets_missed;

    /* Network */
    uint16_t hub_addr;
    uint8_t num_nodes;
    mesh_stats_t mesh_stats;

    /* Audio */
    spl_calculator_t spl_calc;
    float current_spl_dba;
    float current_spl_dbc;
    float current_spl_dbz;

    /* Dose */
    float daily_dose_pct;
    float twa_dba;
    float peak_dba;

    /* Masking */
    masking_mode_t active_masking;
    uint8_t masking_volume;
    uint8_t masking_room;
    bool masking_adaptive;

    /* Nodes */
    struct {
        uint16_t addr;
        mesh_node_type_t type;
        uint8_t room_id;
        bool online;
        uint32_t last_seen_sec;
        uint16_t battery_mv;
    } nodes[MESH_MAX_NODES];

    /* Tinnitus profile */
    tinnitus_profile_t tinnitus_profile;
} hub_state_t;

static hub_state_t g_state;
static SemaphoreHandle_t g_state_mutex;

/* ── Event Queues ────────────────────────────────────────────────────── */

static QueueHandle_t g_event_queue;    /* Sound events from sensors */
static QueueHandle_t g_spl_queue;      /* SPL readings from sensors */
static QueueHandle_t g_alert_queue;    /* Alert commands to wearables */
static QueueHandle_t g_masking_queue;  /* Masking commands to speakers */

/* ── Event Group ────────────────────────────────────────────────────── */

#define EVENT_WIFI_CONNECTED    (1 << 0)
#define EVENT_MQTT_CONNECTED    (1 << 1)
#define EVENT_BLE_CONNECTED     (1 << 2)
#define EVENT_MESH_READY        (1 << 3)
#define EVENT_AUDIO_READY       (1 << 4)
#define EVENT_DISPLAY_READY     (1 << 5)
#define EVENT_ALL_READY         (EVENT_WIFI_CONNECTED | EVENT_MQTT_CONNECTED | \
                                 EVENT_MESH_READY)

static EventGroupHandle_t g_events;

/* ── Forward Declarations ───────────────────────────────────────────── */

static void task_mesh(void *pvParameters);
static void task_audio_ml(void *pvParameters);
static void task_mqtt(void *pvParameters);
static void task_ble(void *pvParameters);
static void task_display(void *pvParameters);
static void task_alarm(void *pvParameters);
static void task_dose(void *pvParameters);
static void task_masking(void *pvParameters);

static void handle_event_report(const mesh_packet_t *pkt);
static void handle_spl_report(const mesh_packet_t *pkt);
static void handle_dose_report(const mesh_packet_t *pkt);
static void handle_masking_feedback(const mesh_packet_t *pkt);
static void handle_heartbeat(const mesh_packet_t *pkt);
static void handle_join_request(const mesh_packet_t *pkt);

static void update_display(void);
static void check_alerts(const event_report_payload_t *event);
static void send_masking_cmd(uint16_t addr, masking_mode_t mode,
                              uint8_t volume, uint8_t room_id);
static void send_alert_cmd(uint16_t addr, alert_priority_t priority,
                            uint8_t sound_class, uint8_t spl_dba);

/* ── Hardware Initialization ─────────────────────────────────────────── */

static esp_err_t init_gpio(void)
{
    /* Configure output pins */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_PI_BEEPER) | (1ULL << GPIO_SPK_EN) |
                        (1ULL << GPIO_LED_DATA) | (1ULL << GPIO_BL_TFT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    /* Configure input pins */
    io_conf.pin_bit_mask = (1ULL << GPIO_BUTTON) | (1ULL << GPIO_VBUS_SENSE);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    /* Initialize outputs */
    gpio_set_level(GPIO_SPK_EN, 0);  /* Speaker amp off initially */
    gpio_set_level(GPIO_BL_TFT, 1);  /* TFT backlight on */
    gpio_set_level(GPIO_PI_BEEPER, 0);

    ESP_LOGI(TAG, "GPIO initialized");
    return ESP_OK;
}

static esp_err_t init_i2c(void)
{
    /* I2C bus for ES8388 codec, DS3231 RTC, Qwiic expansion */
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_I2C_SDA,
        .scl_io_num = GPIO_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,  /* 400kHz */
    };
    i2c_param_config(I2C_NUM_0, &i2c_conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    ESP_LOGI(TAG, "I2C initialized");
    return ESP_OK;
}

static esp_err_t init_i2s(void)
{
    /* I2S for ES8388 audio codec */
    i2s_chan_handle_t rx_chan;
    i2s_chan_handle_t tx_chan;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                         I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = GPIO_I2S_MCLK,  /* Use a GPIO or set to -1 for auto */
            .bclk = GPIO_I2S_BCLK,
            .ws = GPIO_I2S_LRCLK,
            .dout = GPIO_I2S_DOUT,
            .din = GPIO_I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    i2s_channel_init_std_mode(tx_chan, &std_cfg);
    i2s_channel_init_std_mode(rx_chan, &std_cfg);
    i2s_channel_enable(tx_chan);
    i2s_channel_enable(rx_chan);

    ESP_LOGI(TAG, "I2S initialized (16kHz, 32-bit, stereo)");
    return ESP_OK;
}

static esp_err_t init_spi(void)
{
    /* SPI2 for SD card and TFT display */
    spi_bus_config_t spi2_cfg = {
        .mosi_io_num = GPIO_SPI_MOSI,
        .miso_io_num = GPIO_SPI_MISO,
        .sclk_io_num = GPIO_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    spi_bus_initialize(SPI2_HOST, &spi2_cfg, SPI_DMA_CH_AUTO);

    ESP_LOGI(TAG, "SPI2 initialized");
    return ESP_OK;
}

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_LOGI(TAG, "NVS initialized");
    return ret;
}

/* ── Main Application ────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║     SoundNest Hub Node v1.0         ║");
    ESP_LOGI(TAG, "║     AI Acoustic Intelligence         ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════╝");

    /* Initialize global state */
    memset(&g_state, 0, sizeof(g_state));
    g_state.hub_addr = 0x0001;  /* Hub always has address 0x0001 */
    g_state_mutex = xSemaphoreCreateMutex();

    /* Create event queues */
    g_event_queue = xQueueCreate(QUEUE_SIZE_EVENTS, sizeof(event_report_payload_t));
    g_spl_queue = xQueueCreate(QUEUE_SIZE_SPL, sizeof(spl_report_payload_t));
    g_alert_queue = xQueueCreate(QUEUE_SIZE_ALERTS, sizeof(alert_cmd_payload_t));
    g_masking_queue = xQueueCreate(QUEUE_SIZE_MASKING, sizeof(masking_cmd_payload_t));
    g_events = xEventGroupCreate();

    /* Initialize hardware */
    init_nvs();
    init_gpio();
    init_i2c();
    init_i2s();
    init_spi();

    /* Initialize SPL calculator */
    spl_init(&g_state.spl_calc);

    /* Initialize subsystems */
    mesh_manager_init();
    audio_ml_init();
    display_init();
    alarm_manager_init();
    dose_calculator_init();
    masking_engine_init();
    tinnitus_init();
    ota_manager_init();

    /* Create FreeRTOS tasks */
    xTaskCreate(task_mesh, "mesh", STACK_MESH, NULL, TASK_PRIORITY_MESH, NULL);
    xTaskCreate(task_audio_ml, "audio_ml", STACK_AUDIO_ML, NULL,
                TASK_PRIORITY_AUDIO_ML, NULL);
    xTaskCreate(task_alarm, "alarm", STACK_ALARM, NULL, TASK_PRIORITY_ALARM, NULL);
    xTaskCreate(task_dose, "dose", STACK_DOSE, NULL, TASK_PRIORITY_DOSE, NULL);
    xTaskCreate(task_masking, "masking", STACK_MASKING, NULL,
                TASK_PRIORITY_MASKING, NULL);

    /* Wait for mesh to be ready before starting network tasks */
    xEventGroupWaitBits(g_events, EVENT_MESH_READY, false, true,
                         pdMS_TO_TICKS(30000));

    /* Start WiFi and MQTT (these take longer) */
    xTaskCreate(task_mqtt, "mqtt", STACK_MQTT, NULL, TASK_PRIORITY_MQTT, NULL);
    xTaskCreate(task_ble, "ble", STACK_BLE, NULL, TASK_PRIORITY_BLE, NULL);

    /* Start display last (it needs data from other tasks) */
    xTaskCreate(task_display, "display", STACK_DISPLAY, NULL,
                TASK_PRIORITY_DISPLAY, NULL);

    ESP_LOGI(TAG, "All tasks started. SoundNest Hub is running.");

    /* Main loop: periodic housekeeping */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        g_state.uptime_sec++;

        /* Send heartbeat to all nodes every 30 seconds */
        if (g_state.uptime_sec % 30 == 0) {
            heartbeat_payload_t hb = {
                .node_type = NODE_TYPE_HUB,
                .battery_mv = 0,  /* Hub is always powered */
                .rssi = 0,
                .status = 0x07,  /* All subsystems OK */
                .uptime_sec = g_state.uptime_sec,
                .events_today = g_state.events_today,
                .packets_sent = g_state.packets_sent,
                .packets_missed = g_state.packets_missed,
            };
            uint8_t buf[128];
            int len = mesh_build_heartbeat(g_state.hub_addr, 0xFFFF, /* broadcast */
                                           g_state.packets_sent++, &hb,
                                           buf, sizeof(buf));
            if (len > 0) {
                mesh_manager_send(buf, len);
            }
        }

        /* Check for offline nodes every 60 seconds */
        if (g_state.uptime_sec % 60 == 0) {
            for (int i = 0; i < MESH_MAX_NODES; i++) {
                if (g_state.nodes[i].online) {
                    uint32_t since_last = g_state.uptime_sec -
                                          g_state.nodes[i].last_seen_sec;
                    if (since_last > 120) {  /* 2 minutes = offline */
                        g_state.nodes[i].online = false;
                        ESP_LOGW(TAG, "Node 0x%04x went offline", g_state.nodes[i].addr);
                    }
                }
            }
        }
    }
}

/* ── Mesh Task ────────────────────────────────────────────────────────── */

static void task_mesh(void *pvParameters)
{
    ESP_LOGI(TAG, "Mesh task started");
    mesh_packet_t pkt;

    /* Start mesh coordinator */
    mesh_manager_start_coordinator();

    /* Signal mesh ready */
    xEventGroupSetBits(g_events, EVENT_MESH_READY);

    while (1) {
        /* Receive mesh packets */
        if (mesh_manager_receive(&pkt, pdMS_TO_TICKS(100)) == 0) {
            /* Route packet based on message type */
            switch (pkt.header.msg_type) {
            case MSG_TYPE_JOIN_REQ:
                handle_join_request(&pkt);
                break;
            case MSG_TYPE_EVENT_REPORT:
                handle_event_report(&pkt);
                break;
            case MSG_TYPE_SPL_REPORT:
                handle_spl_report(&pkt);
                break;
            case MSG_TYPE_DOSE_REPORT:
                handle_dose_report(&pkt);
                break;
            case MSG_TYPE_MASKING_FEEDBACK:
                handle_masking_feedback(&pkt);
                break;
            case MSG_TYPE_HEARTBEAT:
                handle_heartbeat(&pkt);
                break;
            default:
                ESP_LOGD(TAG, "Unknown message type: 0x%02x", pkt.header.msg_type);
                break;
            }
        }

        /* Process outgoing masking commands */
        masking_cmd_payload_t masking_cmd;
        if (xQueueReceive(g_masking_queue, &masking_cmd, 0) == pdPASS) {
            uint8_t buf[128];
            int len = mesh_build_masking_cmd(g_state.hub_addr,
                                              masking_cmd.volume, /* dst */
                                              g_state.packets_sent++,
                                              &masking_cmd, buf, sizeof(buf));
            if (len > 0) {
                mesh_manager_send(buf, len);
            }
        }

        /* Process outgoing alert commands */
        alert_cmd_payload_t alert_cmd;
        if (xQueueReceive(g_alert_queue, &alert_cmd, 0) == pdPASS) {
            uint8_t buf[128];
            int len = mesh_build_alert_cmd(g_state.hub_addr,
                                            0, /* dst filled from cmd */
                                            g_state.packets_sent++,
                                            &alert_cmd, buf, sizeof(buf));
            if (len > 0) {
                mesh_manager_send(buf, len);
            }
        }
    }
}

/* ── Audio ML Task ────────────────────────────────────────────────────── */

static void task_audio_ml(void *pvParameters)
{
    ESP_LOGI(TAG, "Audio ML task started");

    /* Audio buffer for hub microphone */
    int16_t *audio_buf = malloc(16000 * 2);  /* 1 second at 16kHz */
    float *float_buf = malloc(16000 * sizeof(float));

    if (!audio_buf || !float_buf) {
        ESP_LOGE(TAG, "Failed to allocate audio buffers");
        vTaskDelete(NULL);
    }

    while (1) {
        /* Read audio from hub microphone (ES8388) */
        size_t bytes_read = 0;
        esp_err_t ret = i2s_channel_read(g_i2s_rx_chan, audio_buf,
                                           16000 * 2, &bytes_read,
                                           pdMS_TO_TICKS(1000));
        if (ret != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        int num_samples = bytes_read / 2;

        /* Convert to float */
        for (int i = 0; i < num_samples; i++) {
            float_buf[i] = (float)audio_buf[i] / 32768.0f;
        }

        /* Run SPL calculation */
        spl_result_t spl_result;
        spl_process(&g_state.spl_calc, audio_buf, num_samples, &spl_result);

        xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100));
        g_state.current_spl_dba = spl_result.spl_dba;
        g_state.current_spl_dbc = spl_result.spl_dbc;
        g_state.current_spl_dbz = spl_result.spl_dbz;
        xSemaphoreGive(g_state_mutex);

        /* Run sound classification */
        uint8_t sound_class;
        uint8_t confidence;
        if (audio_ml_classify(float_buf, num_samples, &sound_class, &confidence) == 0) {
            if (sound_class != SOUND_SILENCE && confidence > 50) {
                /* Significant sound event detected at hub */
                event_report_payload_t event = {
                    .sound_class = sound_class,
                    .confidence = confidence,
                    .direction_deg = 0,  /* No direction at hub */
                    .spl_dba = (uint8_t)spl_result.spl_dba,
                    .spl_dbc = (uint8_t)spl_result.spl_dbc,
                    .spl_dbz = (uint8_t)spl_result.spl_dbz,
                    .peak_spl = (uint8_t)spl_result.spl_peak_dba,
                    .duration_ms = 2000,
                    .timestamp = (uint32_t)(esp_timer_get_time() / 1000),
                    .room_id = 0,  /* Hub room */
                    .occupancy = 1,
                    .temp_c = 22.0f,  /* Would come from sensor */
                    .humidity_pct = 50,
                };

                xQueueSend(g_event_queue, &event, pdMS_TO_TICKS(100));
                g_state.events_today++;

                /* Check if this event needs to trigger alerts */
                check_alerts(&event);
            }
        }
    }
}

/* ── MQTT Task ────────────────────────────────────────────────────────── */

static void task_mqtt(void *pvParameters)
{
    ESP_LOGI(TAG, "MQTT task started");

    /* Initialize WiFi */
    mqtt_manager_init();

    /* Wait for WiFi connection */
    xEventGroupWaitBits(g_events, EVENT_WIFI_CONNECTED, false, true,
                         portMAX_DELAY);

    /* Connect to MQTT broker */
    mqtt_manager_connect();

    while (1) {
        /* Publish SPL readings every 5 seconds */
        static int spl_counter = 0;
        if (++spl_counter >= 5) {
            spl_counter = 0;
            char payload[256];
            snprintf(payload, sizeof(payload),
                     "{\"hub\":\"%04x\",\"dba\":%.1f,\"dbc\":%.1f,\"dbz\":%.1f,"
                     "\"ts\":%lld}",
                     g_state.hub_addr,
                     g_state.current_spl_dba,
                     g_state.current_spl_dbc,
                     g_state.current_spl_dbz,
                     esp_timer_get_time() / 1000);
            mqtt_manager_publish("soundnest/hub/spl", payload, 0, 1);
        }

        /* Publish events from queue */
        event_report_payload_t event;
        if (xQueueReceive(g_event_queue, &event, pdMS_TO_TICKS(100)) == pdPASS) {
            char payload[512];
            snprintf(payload, sizeof(payload),
                     "{\"hub\":\"%04x\",\"class\":%d,\"conf\":%d,"
                     "\"dir\":%d,\"dba\":%d,\"peak\":%d,\"dur\":%d,"
                     "\"room\":%d,\"occ\":%d,\"ts\":%u}",
                     g_state.hub_addr,
                     event.sound_class,
                     event.confidence,
                     event.direction_deg,
                     event.spl_dba,
                     event.peak_spl,
                     event.duration_ms,
                     event.room_id,
                     event.occupancy,
                     event.timestamp);
            mqtt_manager_publish("soundnest/hub/events", payload, 0, 1);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ── BLE Task ─────────────────────────────────────────────────────────── */

static void task_ble(void *pvParameters)
{
    ESP_LOGI(TAG, "BLE task started");
    ble_gatt_init();
    ble_gatt_start_advertising();

    while (1) {
        /* Update BLE characteristics */
        ble_gatt_update_spl(g_state.current_spl_dba,
                           g_state.current_spl_dbc,
                           g_state.current_spl_dbz);
        ble_gatt_update_dose(g_state.daily_dose_pct);
        ble_gatt_update_masking(g_state.active_masking, g_state.masking_volume);

        /* Handle BLE commands from mobile app */
        ble_cmd_t cmd;
        if (ble_gatt_get_command(&cmd, pdMS_TO_TICKS(500)) == pdPASS) {
            switch (cmd.type) {
            case BLE_CMD_MASKING_START:
                send_masking_cmd(0xFFFF, cmd.masking_mode, cmd.volume, cmd.room_id);
                break;
            case BLE_CMD_MASKING_STOP:
                send_masking_cmd(0xFFFF, MASKING_OFF, 0, cmd.room_id);
                break;
            case BLE_CMD_MASKING_ADJUST:
                send_masking_cmd(0xFFFF, cmd.masking_mode, cmd.volume, cmd.room_id);
                break;
            default:
                break;
            }
        }
    }
}

/* ── Display Task ─────────────────────────────────────────────────────── */

static void task_display(void *pvParameters)
{
    ESP_LOGI(TAG, "Display task started");

    while (1) {
        update_display();
        vTaskDelay(pdMS_TO_TICKS(500));  /* 2 FPS display update */
    }
}

/* ── Alarm Task ───────────────────────────────────────────────────────── */

static void task_alarm(void *pvParameters)
{
    ESP_LOGI(TAG, "Alarm task started");

    while (1) {
        alarm_event_t alarm;
        if (alarm_manager_get_event(&alarm, portMAX_DELAY) == pdPASS) {
            /* Activate piezo buzzer */
            if (alarm.priority >= ALERT_PRIORITY_HIGH) {
                gpio_set_level(GPIO_PI_BEEPER, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(GPIO_PI_BEEPER, 0);

                if (alarm.priority >= ALERT_PRIORITY_CRITICAL) {
                    vTaskDelay(pdMS_TO_TICKS(200));
                    gpio_set_level(GPIO_PI_BEEPER, 1);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    gpio_set_level(GPIO_PI_BEEPER, 0);
                }
            }

            /* Activate speaker for TTS alarm */
            if (alarm.priority >= ALERT_PRIORITY_MEDIUM) {
                /* Play alarm sound through ES8388 + speaker */
                gpio_set_level(GPIO_SPK_EN, 1);
                /* TTS or tone generation would go here */
                vTaskDelay(pdMS_TO_TICKS(1000));
                gpio_set_level(GPIO_SPK_EN, 0);
            }

            /* Flash RGB LEDs */
            uint8_t r = 0, g = 0, b = 0;
            switch (alarm.priority) {
            case ALERT_PRIORITY_INFO:      g = 255; break;  /* Green */
            case ALERT_PRIORITY_LOW:       r = 255; g = 165; break;  /* Orange */
            case ALERT_PRIORITY_MEDIUM:    r = 255; g = 255; break;  /* Yellow */
            case ALERT_PRIORITY_HIGH:      r = 255; break;  /* Red */
            case ALERT_PRIORITY_CRITICAL:  r = 255; break;  /* Red flash */
            }
            /* WS2812B LED update would go here */
        }
    }
}

/* ── Dose Calculation Task ────────────────────────────────────────────── */

static void task_dose(void *pvParameters)
{
    ESP_LOGI(TAG, "Dose task started");
    float exposure_time_sec = 0;
    float accumulated_dose = 0;

    while (1) {
        /* Accumulate dose from current SPL every second */
        xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100));

        float current_dba = g_state.current_spl_dba;
        float dose_increment = spl_calculate_dose(current_dba, 1.0f / 3600.0f);
        accumulated_dose += dose_increment;
        exposure_time_sec += 1.0f;

        g_state.daily_dose_pct = accumulated_dose;
        g_state.twa_dba = 10.0f * log10f(
            accumulated_dose > 0 ? powf(10, current_dba / 10.0f) : 1e-10f);
        if (current_dba > g_state.peak_dba) {
            g_state.peak_dba = current_dba;
        }

        xSemaphoreGive(g_state_mutex);

        /* Publish dose update via MQTT every 30 seconds */
        if (((uint32_t)exposure_time_sec) % 30 == 0) {
            char payload[256];
            snprintf(payload, sizeof(payload),
                     "{\"hub\":\"%04x\",\"dose_pct\":%.1f,\"twa_dba\":%.1f,"
                     "\"peak_dba\":%.1f,\"exposure_min\":%.0f,\"ts\":%lld}",
                     g_state.hub_addr,
                     g_state.daily_dose_pct,
                     g_state.twa_dba,
                     g_state.peak_dba,
                     exposure_time_sec / 60.0f,
                     esp_timer_get_time() / 1000);
            mqtt_manager_publish("soundnest/hub/dose", payload, 0, 1);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ── Masking Task ─────────────────────────────────────────────────────── */

static void task_masking(void *pvParameters)
{
    ESP_LOGI(TAG, "Masking task started");

    while (1) {
        /* Process masking commands from MQTT or BLE */
        /* If adaptive masking is enabled, adjust volume based on ambient noise */
        if (g_state.masking_adaptive && g_state.active_masking != MASKING_OFF) {
            /* Simple adaptive algorithm:
             * Target masking level = ambient SPL + 10 dB
             * Volume maps 0-100% to 30-80 dB SPL at 1m */
            float target_dba = g_state.current_spl_dba + 10.0f;
            float current_masking_dba = 30.0f + (g_state.masking_volume * 0.5f);

            if (target_dba > current_masking_dba + 5.0f) {
                /* Increase volume */
                g_state.masking_volume = (uint8_t)(g_state.masking_volume < 100 ?
                    g_state.masking_volume + 1 : 100);
            } else if (target_dba < current_masking_dba - 5.0f) {
                /* Decrease volume */
                g_state.masking_volume = (uint8_t)(g_state.masking_volume > 10 ?
                    g_state.masking_volume - 1 : 10);
            }

            /* Send updated masking command to speakers */
            masking_cmd_payload_t cmd = {
                .mode = g_state.active_masking,
                .volume = g_state.masking_volume,
                .stereo_balance = 50,  /* Center */
                .fade_in_ms = 0,
                .fade_out_ms = 0,
                .duration_min = 0,
                .adaptive = 1,
            };
            send_masking_cmd(0xFFFF, cmd.mode, cmd.volume, g_state.masking_room);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));  /* Adjust every 5 seconds */
    }
}

/* ── Event Handlers ───────────────────────────────────────────────────── */

static void handle_join_request(const mesh_packet_t *pkt)
{
    join_req_payload_t *join = (join_req_payload_t *)pkt->payload;

    ESP_LOGI(TAG, "Join request from node type %d, firmware v%d.%d.%d.%d",
             join->node_type,
             join->firmware_ver[0], join->firmware_ver[1],
             join->firmware_ver[2], join->firmware_ver[3]);

    /* Find a free node slot */
    int slot = -1;
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (!g_state.nodes[i].online && g_state.nodes[i].addr == 0) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        ESP_LOGW(TAG, "No free node slots, rejecting join");
        /* Send JOIN_NACK */
        return;
    }

    /* Assign address and slot */
    uint16_t assigned_addr = 0x0002 + slot;
    g_state.nodes[slot].addr = assigned_addr;
    g_state.nodes[slot].type = join->node_type;
    g_state.nodes[slot].online = true;
    g_state.nodes[slot].last_seen_sec = g_state.uptime_sec;
    g_state.nodes[slot].battery_mv = 0;
    g_state.num_nodes++;

    /* Send JOIN_ACK */
    join_ack_payload_t ack = {
        .assigned_addr = assigned_addr,
        .tdma_slot = (uint8_t)(slot + 1),
        .sf_config = RADIO_SF7BW125,
        .channel = 0,
    };
    mesh_crypto_derive_session_key(g_network_key, assigned_addr, ack.encryption_key);

    uint8_t buf[128];
    int len = mesh_packet_encode(&pkt->header, (const uint8_t *)&ack,
                                  sizeof(join_ack_payload_t), buf, sizeof(buf));
    if (len > 0) {
        mesh_manager_send(buf, len);
    }

    ESP_LOGI(TAG, "Node 0x%04x assigned (slot %d, type %d)",
             assigned_addr, slot + 1, join->node_type);
}

static void handle_event_report(const mesh_packet_t *pkt)
{
    event_report_payload_t *event = (event_report_payload_t *)pkt->payload;

    ESP_LOGI(TAG, "Sound event: %s (confidence %d%%, %.1f dBA, direction %d°, room %d)",
             mesh_sound_event_name(event->sound_class),
             event->confidence,
             (float)event->spl_dba,
             event->direction_deg,
             event->room_id);

    /* Forward to event queue for MQTT publishing */
    xQueueSend(g_event_queue, event, pdMS_TO_TICKS(100));

    /* Check if this event needs to trigger alerts */
    check_alerts(event);
}

static void handle_spl_report(const mesh_packet_t *pkt)
{
    spl_report_payload_t *spl = (spl_report_payload_t *)pkt->payload;

    /* Update node status */
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (g_state.nodes[i].addr == pkt->header.src_addr) {
            g_state.nodes[i].last_seen_sec = g_state.uptime_sec;
            g_state.nodes[i].battery_mv = spl->battery_mv;
            break;
        }
    }

    /* Forward to MQTT */
    char payload[512];
    snprintf(payload, sizeof(payload),
             "{\"node\":\"%04x\",\"dba\":%d,\"dbc\":%d,\"dbz\":%d,"
             "\"min\":%d,\"max\":%d,\"eq\":%d,\"occ\":%d,"
             "\"temp\":%.1f,\"hum\":%d,\"bat\":%d,\"ts\":%u}",
             pkt->header.src_addr,
             spl->spl_dba, spl->spl_dbc, spl->spl_dbz,
             spl->spl_min, spl->spl_max, spl->spl_eq,
             spl->occupancy,
             spl->temp_c, spl->humidity_pct,
             spl->battery_mv,
             spl->timestamp);
    mqtt_manager_publish("soundnest/sensor/spl", payload, 0, 1);

    /* Queue for display update */
    xQueueSend(g_spl_queue, spl, pdMS_TO_TICKS(100));
}

static void handle_dose_report(const mesh_packet_t *pkt)
{
    dose_report_payload_t *dose = (dose_report_payload_t *)pkt->payload;

    /* Forward to MQTT */
    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"node\":\"%04x\",\"dose_pct\":%d,\"dba\":%d,"
             "\"twa\":%.1f,\"peak\":%d,\"exposure_min\":%d,"
             "\"activity\":%d,\"bat\":%d,\"ts\":%u}",
             pkt->header.src_addr,
             dose->daily_dose_pct, dose->current_spl_dba,
             dose->twa_dba_x10 / 10.0f, dose->peak_dba,
             dose->exposure_min, dose->activity,
             dose->battery_mv, dose->timestamp);
    mqtt_manager_publish("soundnest/wearable/dose", payload, 0, 1);
}

static void handle_masking_feedback(const mesh_packet_t *pkt)
{
    masking_feedback_payload_t *fb = (masking_feedback_payload_t *)pkt->payload;
    /* Update masking state based on speaker feedback */
    ESP_LOGD(TAG, "Masking feedback from 0x%04x: mode=%d, vol=%d",
             pkt->header.src_addr, fb->mode, fb->volume);
}

static void handle_heartbeat(const mesh_packet_t *pkt)
{
    heartbeat_payload_t *hb = (heartbeat_payload_t *)pkt->payload;

    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (g_state.nodes[i].addr == pkt->header.src_addr) {
            g_state.nodes[i].online = true;
            g_state.nodes[i].last_seen_sec = g_state.uptime_sec;
            g_state.nodes[i].battery_mv = hb->battery_mv;
            break;
        }
    }
}

/* ── Alert Logic ──────────────────────────────────────────────────────── */

static void check_alerts(const event_report_payload_t *event)
{
    /* Critical alerts (fire, CO, glass break) — alert all wearables */
    if (event->sound_class == SOUND_SMOKE_ALARM ||
        event->sound_class == SOUND_CO_ALARM ||
        event->sound_class == SOUND_GLASS_BREAK ||
        event->sound_class == SOUND_GUNSHOT) {
        send_alert_cmd(0xFFFF, /* broadcast to all wearables */
                       ALERT_PRIORITY_CRITICAL,
                       event->sound_class,
                       event->spl_dba);
    }

    /* High priority (doorbell, baby crying, phone) */
    if (event->sound_class == SOUND_DOORBELL ||
        event->sound_class == SOUND_CRYING_BABY ||
        event->sound_class == SOUND_PHONE_RING ||
        event->sound_class == SOUND_SIREN) {
        send_alert_cmd(0xFFFF,
                       ALERT_PRIORITY_HIGH,
                       event->sound_class,
                       event->spl_dba);
    }

    /* Medium priority (door knock, door open, dog bark) */
    if (event->sound_class == SOUND_DOOR_KNOCK ||
        event->sound_class == SOUND_DOOR_OPEN ||
        event->sound_class == SOUND_DOG_BARK ||
        event->sound_class == SOUND_NOTIFICATION) {
        send_alert_cmd(0xFFFF,
                       ALERT_PRIORITY_MEDIUM,
                       event->sound_class,
                       event->spl_dba);
    }

    /* Auto-masking triggers (loud speech, TV, music) */
    if (event->sound_class == SOUND_SPEECH && event->spl_dba > 65) {
        /* Auto-start privacy masking if speech is loud */
        masking_cmd_payload_t cmd = {
            .mode = MASKING_PRIVACY,
            .volume = 60,
            .stereo_balance = 50,
            .fade_in_ms = 20,  /* 2 second fade in */
            .adaptive = 1,
            .duration_min = 30,
        };
        xQueueSend(g_masking_queue, &cmd, pdMS_TO_TICKS(100));
    }

    if ((event->sound_class == SOUND_TV || event->sound_class == SOUND_MUSIC) &&
        event->spl_dba > 70) {
        /* Suggest masking for loud media */
        masking_cmd_payload_t cmd = {
            .mode = MASKING_PINK_NOISE,
            .volume = 50,
            .stereo_balance = 50,
            .fade_in_ms = 30,  /* 3 second fade in */
            .adaptive = 1,
            .duration_min = 60,
        };
        xQueueSend(g_masking_queue, &cmd, pdMS_TO_TICKS(100));
    }
}

/* ── Helper Functions ─────────────────────────────────────────────────── */

static void send_masking_cmd(uint16_t addr, masking_mode_t mode,
                              uint8_t volume, uint8_t room_id)
{
    masking_cmd_payload_t cmd = {
        .mode = mode,
        .volume = volume,
        .stereo_balance = 50,
        .freq_hz = {0, 0},
        .bandwidth = 0,
        .fade_in_ms = 30,
        .fade_out_ms = 50,
        .duration_min = 0,
        .adaptive = g_state.masking_adaptive ? 1 : 0,
        .reserved = {0},
    };
    xQueueSend(g_masking_queue, &cmd, pdMS_TO_TICKS(100));
}

static void send_alert_cmd(uint16_t addr, alert_priority_t priority,
                            uint8_t sound_class, uint8_t spl_dba)
{
    alert_cmd_payload_t cmd = {
        .priority = priority,
        .sound_class = sound_class,
        .haptic_pattern = (uint8_t)priority,  /* Pattern matches priority */
        .led_color = 0,  /* Will be set based on priority */
        .led_pattern = (uint8_t)priority,
        .spl_dba = spl_dba,
        .direction = 0,
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000),
        .message_len = 0,
        .message = {0},
    };

    /* Set LED color based on priority */
    switch (priority) {
    case ALERT_PRIORITY_INFO:      cmd.led_color = 0x04; break;  /* Green */
    case ALERT_PRIORITY_LOW:       cmd.led_color = 0x1C; break;  /* Orange */
    case ALERT_PRIORITY_MEDIUM:    cmd.led_color = 0x1E; break;  /* Yellow */
    case ALERT_PRIORITY_HIGH:      cmd.led_color = 0x07; break;  /* Red */
    case ALERT_PRIORITY_CRITICAL:  cmd.led_color = 0x07; break;  /* Red flash */
    }

    /* Copy event name */
    const char *name = mesh_sound_event_name(sound_class);
    strncpy(cmd.message, name, sizeof(cmd.message) - 1);
    cmd.message_len = (uint8_t)strlen(name);

    xQueueSend(g_alert_queue, &cmd, pdMS_TO_TICKS(100));

    /* Also trigger local alarm */
    alarm_event_t alarm = {
        .priority = priority,
        .sound_class = sound_class,
        .spl_dba = spl_dba,
    };
    alarm_manager_trigger(&alarm);
}

static void update_display(void)
{
    /* Render LVGL display:
     * - Top: Current SPL (dB(A)) with color-coded gauge
     * - Middle: Active alerts / recent events
     * - Bottom: Dose %, masking status, node status
     * - Side bar: Miniature acoustic timeline
     */
    display_update_spl(g_state.current_spl_dba);
    display_update_dose(g_state.daily_dose_pct);
    display_update_masking(g_state.active_masking, g_state.masking_volume);
    display_update_nodes(g_state.nodes, g_state.num_nodes);
}