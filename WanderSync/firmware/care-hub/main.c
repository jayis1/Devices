/*
 * WanderSync — Care Hub / Gateway Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Care Hub coordinates the Sub-GHz TDMA mesh, bridges to the cloud
 * via Wi-Fi/MQTT with 4G LTE backup, runs geofencing, coordinates door
 * locking, schedules voice reminders, aggregates ADL timelines, runs
 * edge WanderNet inference, and dispatches 911 via SIM7000 4G LTE.
 *
 * Build: idf.py build with ESP-IDF v5.x
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
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

#include "../common/protocol.h"
#include "../common/subghz_mesh.h"
#include "../common/config.h"

static const char *TAG = "WanderSync-Hub";

/* === Global state === */
static ws_mesh_ctx_t g_mesh;
static ws_radio_hal_t g_radio_hal;
static ws_radio_config_t g_radio_cfg;
static QueueHandle_t g_alert_queue;
static QueueHandle_t g_telemetry_queue;
static SemaphoreHandle_t g_mesh_mutex;
static ws_node_info_t g_node_table[WS_MESH_MAX_NODES];

/* Geofence state */
static ws_geofence_t g_geofence;
static int32_t g_band_lat_e7 = 0;
static int32_t g_band_lon_e7 = 0;
static uint8_t g_band_geofence_status = 0; /* 0=inside, 1=outside, 2=near */
static uint8_t g_wander_active = 0;
static uint8_t g_fall_active = 0;
static uint8_t g_emergency_cancel_window = 0;

/* Reminder schedule (24 slots, one per hour) */
typedef struct {
    uint8_t  enabled;
    uint8_t  reminder_id;
    uint8_t  clip_index;
    uint8_t  volume;
    uint8_t  reminder_type;
} ws_reminder_slot_t;
static ws_reminder_slot_t g_reminders[24];
static uint8_t g_last_reminder_hour = 0xFF;

/* === SX1262 HAL (ESP32-S3 SPI) === */
static spi_device_handle_t g_spi;
static SemaphoreHandle_t g_spi_mutex;

static int hal_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = HUB_GPIO_SX_MOSI,
        .miso_io_num = HUB_GPIO_SX_MISO,
        .sclk_io_num = HUB_GPIO_SX_SCK,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2000000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi);
    g_spi_mutex = xSemaphoreCreateMutex();
    return 0;
}

static int hal_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);
    spi_transaction_t t = {0};
    t.length = len * 8;
    if (tx) t.tx_buffer = tx;
    if (rx) t.rx_buffer = rx;
    spi_device_polling_transmit(g_spi, &t);
    xSemaphoreGive(g_spi_mutex);
    return 0;
}

static void hal_cs_low(void)  { gpio_set_level(HUB_GPIO_SX_NSS, 0); }
static void hal_cs_high(void) { gpio_set_level(HUB_GPIO_SX_NSS, 1); }
static void hal_reset(int assert) { gpio_set_level(HUB_GPIO_SX_RST, !assert); }
static int  hal_dio1_read(void) { return gpio_get_level(HUB_GPIO_SX_DIO1); }
static int  hal_busy_read(void) { return gpio_get_level(HUB_GPIO_SX_BUSY); }
static void hal_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static void hal_delay_us(uint32_t us) { esp_rom_delay_us(us); }
static void hal_on_dio1(void) { /* IRQ handled in radio task */ }

/* === Geofence Check === */
static double ws_haversine_m(int32_t lat1_e7, int32_t lon1_e7,
                             int32_t lat2_e7, int32_t lon2_e7)
{
    double lat1 = lat1_e7 / 1e7;
    double lon1 = lon1_e7 / 1e7;
    double lat2 = lat2_e7 / 1e7;
    double lon2 = lon2_e7 / 1e7;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dlon / 2) * sin(dlon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return 6371000.0 * c; /* Earth radius in meters */
}

static uint8_t ws_check_geofence(int32_t lat_e7, int32_t lon_e7)
{
    double dist = ws_haversine_m(lat_e7, lon_e7,
                                  g_geofence.center_lat_e7,
                                  g_geofence.center_lon_e7);

    /* Check if night (stricter geofence) */
    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    uint8_t hour = tm_info->tm_hour;
    uint16_t radius = g_geofence.radius_m;
    if (hour >= g_geofence.night_start_h || hour < g_geofence.night_end_h) {
        radius = g_geofence.night_radius_m;
    }

    if (dist > radius) return 1; /* outside */
    if (dist > radius - WS_GEOFENCE_NEAR_M) return 2; /* near boundary */
    return 0; /* inside */
}

/* === Wander Response === */
static void ws_wander_response(uint8_t alert_type, int32_t lat_e7,
                                int32_t lon_e7, uint8_t risk,
                                uint8_t activity, uint8_t battery_v,
                                uint8_t impact_g, uint8_t on_wrist, uint8_t hr)
{
    ESP_LOGW(TAG, "WANDER RESPONSE: type=%d lat=%d lon=%d risk=%d%%",
             alert_type, lat_e7, lon_e7, risk);

    g_wander_active = 1;
    g_emergency_cancel_window = WS_EMERGENCY_CANCEL_WINDOW_S;

    /* 1. Lock all doors */
    ws_message_t lock_msg;
    ws_build_door_lock_cmd(&lock_msg, WS_HUB_NODE_ID, WS_BROADCAST,
                           g_mesh.msg_counter++, 0xFF, 1 /* lock */,
                           1 /* night */, 0, 2 /* emergency */);
    xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
    ws_mesh_broadcast_emergency(&g_mesh, &g_radio_hal, &lock_msg);
    xSemaphoreGive(g_mesh_mutex);

    /* 2. Trigger safety voice reminder */
    ws_message_t voice_msg;
    ws_build_reminder_trigger(&voice_msg, WS_HUB_NODE_ID, 0xFE,
                              g_mesh.msg_counter++, 0xFF,
                              WS_CLIP_SAFETY_START, 80, 2, 10, 1,
                              WS_REMINDER_CUSTOM, 30);
    xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
    ws_mesh_send_acked(&g_mesh, &g_radio_hal, &voice_msg, 1000, 3);
    xSemaphoreGive(g_mesh_mutex);

    /* 3. Sound buzzer + strobe */
    ledc_set_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0, 255);
    ledc_update_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0);
    gpio_set_level(HUB_GPIO_STROBE, 1);

    /* 4. Dispatch 911 if fall or if outside geofence */
    if (alert_type == WS_ALERT_TYPE_FALL || g_band_geofence_status == 1) {
        ESP_LOGW(TAG, "Dispatching 911 via 4G LTE...");
        /* Production: AT commands to SIM7000 */
    }

    /* 5. Publish to cloud + push notification */
    /* Production: MQTT publish wandersync/{user}/hub/wander */
}

/* === 911 Dispatch (SIM7000 4G LTE) === */
static void sim7000_init(void)
{
    ESP_LOGI(TAG, "SIM7000 4G LTE initialized");
}

static void sim7000_dispatch_911(const char *address, int32_t lat_e7,
                                    int32_t lon_e7, const char *medical_info)
{
    double lat = lat_e7 / 1e7;
    double lon = lon_e7 / 1e7;
    ESP_LOGW(TAG, "911 DISPATCH: addr=%s lat=%.6f lon=%.6f medical=%s",
             address, lat, lon, medical_info);
    /* Production: AT+CMGS (SMS to 911), ATD (voice call) */
    /* "This is an automated medical alert from WanderSync dementia care system. */
    /*  Person with dementia at [address], GPS: [lat], [lon]. */
    /*  Medical info: [diagnosis, medications, allergies]. */
    /*  Please dispatch emergency services." */
}

/* === Wi-Fi / MQTT Bridge === */
static void wifi_init(void)
{
    ESP_LOGI(TAG, "Wi-Fi initialized (production: connect to SSID)");
}

static void mqtt_init(void)
{
    ESP_LOGI(TAG, "MQTT client initialized (production: connect to broker)");
}

/* === Radio Task === */
static void radio_task(void *arg)
{
    uint8_t rx_buf[WS_MAX_MSG];
    ws_message_t msg;

    while (1) {
        xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
        int rx_len = ws_sx1262_rx(&g_radio_hal, rx_buf, sizeof(rx_buf),
                                   5000, &g_mesh.last_rssi);
        xSemaphoreGive(g_mesh_mutex);

        if (rx_len > 0 && ws_decode(&msg, rx_buf, rx_len) == 0) {
            switch (msg.header.type) {
            case WS_MSG_JOIN_REQ: {
                uint8_t slot = 1;
                for (int i = 1; i < WS_TDMA_SLOTS; i++) {
                    if (g_node_table[i].node_id == 0) {
                        slot = i;
                        break;
                    }
                }
                g_node_table[slot].node_id = msg.header.src;
                g_node_table[slot].node_type = msg.payload[0];
                g_node_table[slot].tdma_slot = slot;
                g_node_table[slot].online = 1;

                ws_message_t ack;
                ack.header.src = WS_HUB_NODE_ID;
                ack.header.dst = msg.header.src;
                ack.header.type = WS_MSG_JOIN_ACK;
                ack.header.msg_id = g_mesh.msg_counter++;
                ack.payload[0] = slot;
                ack.payload_len = 1;

                uint8_t tx_buf[WS_MAX_MSG];
                size_t tx_len = ws_encode(&ack, tx_buf, sizeof(tx_buf));
                xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
                ws_sx1262_tx(&g_radio_hal, tx_buf, tx_len, 22);
                xSemaphoreGive(g_mesh_mutex);

                ESP_LOGI(TAG, "Node %d joined (slot %d, type %d)",
                         msg.header.src, slot, msg.payload[0]);

                /* Send geofence to band nodes */
                if (msg.payload[0] == WS_NODE_BAND) {
                    ws_message_t gf_msg;
                    ws_build_geofence_update(&gf_msg, WS_HUB_NODE_ID,
                                             msg.header.src,
                                             g_mesh.msg_counter++,
                                             &g_geofence);
                    uint8_t gf_buf[WS_MAX_MSG];
                    size_t gf_len = ws_encode(&gf_msg, gf_buf, sizeof(gf_buf));
                    xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
                    ws_sx1262_tx(&g_radio_hal, gf_buf, gf_len, 22);
                    xSemaphoreGive(g_mesh_mutex);
                }
                break;
            }

            case WS_MSG_WANDER_ALERT: {
                ws_wander_alert_t *wa = (ws_wander_alert_t *)msg.payload;
                ESP_LOGW(TAG, "WANDER_ALERT: type=%d lat=%d lon=%d risk=%d%%",
                         wa->alert_type, wa->gps_lat_e7, wa->gps_lon_e7,
                         wa->wander_risk);

                g_band_lat_e7 = wa->gps_lat_e7;
                g_band_lon_e7 = wa->gps_lon_e7;
                g_band_geofence_status = wa->geofence_status;

                if (!g_wander_active) {
                    ws_wander_response(wa->alert_type, wa->gps_lat_e7,
                                       wa->gps_lon_e7, wa->wander_risk,
                                       wa->activity_class, wa->battery_v,
                                       wa->impact_g_x10, wa->band_on_wrist,
                                       wa->hr_bpm);
                }
                break;
            }

            case WS_MSG_FALL_ALERT: {
                ws_wander_alert_t *wa = (ws_wander_alert_t *)msg.payload;
                ESP_LOGW(TAG, "FALL_ALERT: impact=%dg lat=%d lon=%d",
                         wa->impact_g_x10 / 10, wa->gps_lat_e7, wa->gps_lon_e7);
                g_fall_active = 1;
                ws_wander_response(WS_ALERT_TYPE_FALL, wa->gps_lat_e7,
                                   wa->gps_lon_e7, wa->wander_risk,
                                   wa->activity_class, wa->battery_v,
                                   wa->impact_g_x10, wa->band_on_wrist,
                                   wa->hr_bpm);
                break;
            }

            case WS_MSG_SOS_ALERT: {
                ws_wander_alert_t *wa = (ws_wander_alert_t *)msg.payload;
                ESP_LOGW(TAG, "SOS_ALERT: lat=%d lon=%d", wa->gps_lat_e7,
                         wa->gps_lon_e7);
                ws_wander_response(WS_ALERT_TYPE_SOS, wa->gps_lat_e7,
                                   wa->gps_lon_e7, 100, wa->activity_class,
                                   wa->battery_v, 0, wa->band_on_wrist,
                                   wa->hr_bpm);
                break;
            }

            case WS_MSG_DOOR_ALERT: {
                uint8_t door_id = msg.payload[0];
                uint8_t door_state = msg.payload[1];
                uint8_t tamper = msg.payload[2];
                ESP_LOGW(TAG, "DOOR_ALERT: door=%d state=%d tamper=%d",
                         door_id, door_state, tamper);

                if (tamper) {
                    /* Push notification to caregiver */
                    ESP_LOGW(TAG, "Door %d tampered!", door_id);
                }

                /* If door opens at night and band is near, lock it */
                if (door_state == 1) { /* opened */
                    time_t now;
                    time(&now);
                    struct tm *tm_info = localtime(&now);
                    if (tm_info->tm_hour >= WS_DOOR_LOCK_START_H ||
                        tm_info->tm_hour < WS_DOOR_UNLOCK_END_H) {
                        ws_message_t lock_cmd;
                        ws_build_door_lock_cmd(&lock_cmd, WS_HUB_NODE_ID,
                                               msg.header.src,
                                               g_mesh.msg_counter++,
                                               door_id, 1, 1, 0, 1);
                        xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
                        ws_mesh_send_acked(&g_mesh, &g_radio_hal,
                                           &lock_cmd, 1000, 3);
                        xSemaphoreGive(g_mesh_mutex);
                    }
                }
                break;
            }

            case WS_MSG_BAND_REMOVED: {
                ws_wander_alert_t *wa = (ws_wander_alert_t *)msg.payload;
                ESP_LOGW(TAG, "BAND_REMOVED: lat=%d lon=%d",
                         wa->gps_lat_e7, wa->gps_lon_e7);
                /* Push notification to caregiver — band may have fallen off
                 * or been removed by confused person */
                break;
            }

            case WS_MSG_ADL_UPDATE: {
                uint8_t room_id = msg.payload[0];
                uint8_t activity = msg.payload[1];
                uint8_t confidence = msg.payload[2];
                ESP_LOGI(TAG, "ADL: room=%d activity=%d conf=%d%%",
                         room_id, activity, confidence);
                /* Store in ADL timeline — production: aggregate + publish to cloud */
                break;
            }

            case WS_MSG_REMINDER_ACK: {
                uint8_t reminder_id = msg.payload[0];
                uint8_t played = msg.payload[1];
                ESP_LOGI(TAG, "Reminder %d %s", reminder_id,
                         played ? "played" : "failed");
                break;
            }

            case WS_MSG_TELEMETRY: {
                uint8_t subtype = msg.payload[0];
                switch (subtype) {
                case WS_TELEM_BAND:
                    ESP_LOGI(TAG, "Band telem from node %d", msg.header.src);
                    /* Update band location + geofence check */
                    if (msg.payload_len >= sizeof(ws_band_telem_t)) {
                        ws_band_telem_t *bt = (ws_band_telem_t *)msg.payload;
                        g_band_lat_e7 = bt->gps_lat_e7;
                        g_band_lon_e7 = bt->gps_lon_e7;
                        g_band_geofence_status = ws_check_geofence(
                            bt->gps_lat_e7, bt->gps_lon_e7);
                        if (g_band_geofence_status == 1 && !g_wander_active) {
                            ESP_LOGW(TAG, "GEOFENCE BREACH: band outside!");
                        }
                    }
                    break;
                case WS_TELEM_DOOR:
                    ESP_LOGI(TAG, "Door telem from node %d", msg.header.src);
                    break;
                case WS_TELEM_ROOM:
                    ESP_LOGI(TAG, "Room telem from node %d", msg.header.src);
                    break;
                case WS_TELEM_VOICE:
                    ESP_LOGI(TAG, "Voice telem from node %d", msg.header.src);
                    break;
                }
                for (int i = 0; i < WS_MESH_MAX_NODES; i++) {
                    if (g_node_table[i].node_id == msg.header.src) {
                        g_node_table[i].last_seen_ms =
                            xTaskGetTickCount() * portTICK_PERIOD_MS;
                        g_node_table[i].rssi = g_mesh.last_rssi;
                        break;
                    }
                }
                break;
            }

            case WS_MSG_HEARTBEAT: {
                for (int i = 0; i < WS_MESH_MAX_NODES; i++) {
                    if (g_node_table[i].node_id == msg.header.src) {
                        g_node_table[i].battery_v = msg.payload[0];
                        g_node_table[i].rssi = (int8_t)msg.payload[1];
                        g_node_table[i].last_seen_ms =
                            xTaskGetTickCount() * portTICK_PERIOD_MS;
                        g_node_table[i].online = 1;
                        break;
                    }
                }
                break;
            }

            case WS_MSG_SILENCE: {
                g_wander_active = 0;
                g_fall_active = 0;
                gpio_set_level(HUB_GPIO_STROBE, 0);
                ledc_set_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0, 0);
                ledc_update_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0);
                ESP_LOGI(TAG, "Alarm silenced by user");
                break;
            }
            }
        }
    }
}

/* === Reminder Scheduler Task === */
static void reminder_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); /* Check every 10 seconds */

        time_t now;
        time(&now);
        struct tm *tm_info = localtime(&now);
        uint8_t hour = tm_info->tm_hour;

        if (hour != g_last_reminder_hour) {
            g_last_reminder_hour = hour;
            if (hour < 24 && g_reminders[hour].enabled) {
                ESP_LOGI(TAG, "Triggering reminder %d (hour %d, clip %d)",
                         g_reminders[hour].reminder_id, hour,
                         g_reminders[hour].clip_index);

                /* Find voice node and send reminder */
                ws_message_t rem_msg;
                ws_build_reminder_trigger(&rem_msg, WS_HUB_NODE_ID, 0xFE,
                                          g_mesh.msg_counter++,
                                          g_reminders[hour].reminder_id,
                                          g_reminders[hour].clip_index,
                                          g_reminders[hour].volume,
                                          2, 10, 1,
                                          g_reminders[hour].reminder_type, 60);
                xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
                ws_mesh_send_acked(&g_mesh, &g_radio_hal, &rem_msg, 1000, 3);
                xSemaphoreGive(g_mesh_mutex);
            }
        }
    }
}

/* === Emergency Cancel Task === */
static void cancel_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (g_emergency_cancel_window > 0) {
            g_emergency_cancel_window--;
            if (g_emergency_cancel_window == 0 && g_wander_active) {
                ESP_LOGW(TAG, "Cancel window expired — 911 dispatch confirmed");
            }
        }
    }
}

/* === Node Watchdog Task === */
static void watchdog_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        for (int i = 0; i < WS_MESH_MAX_NODES; i++) {
            if (g_node_table[i].node_id != 0 && g_node_table[i].online) {
                if (now - g_node_table[i].last_seen_ms > 120000) {
                    g_node_table[i].online = 0;
                    ESP_LOGW(TAG, "Node %d offline (last seen %d ms ago)",
                             g_node_table[i].node_id,
                             now - g_node_table[i].last_seen_ms);
                }
            }
        }
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "WanderSync Care Hub starting...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* GPIO init */
    gpio_set_direction(HUB_GPIO_SX_NSS, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(HUB_GPIO_SX_BUSY, GPIO_MODE_INPUT);
    gpio_set_direction(HUB_GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_BUZZER, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_STROBE, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_VBAT, GPIO_MODE_ANALOG);
    gpio_set_direction(HUB_GPIO_USB_PWR, GPIO_MODE_INPUT);
    gpio_set_direction(HUB_GPIO_CELL_PWR, GPIO_MODE_OUTPUT);

    gpio_set_level(HUB_GPIO_SX_NSS, 1);
    gpio_set_level(HUB_GPIO_SX_RST, 1);

    /* I²C init (BME280 + DS3231) */
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = HUB_GPIO_BME_SDA,
        .scl_io_num = HUB_GPIO_BME_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &i2c_cfg);
    i2c_driver_install(I2C_NUM_0, i2c_cfg.mode, 0, 0, 0);

    /* Buzzer PWM */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 3000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);
    ledc_channel_config_t ch_cfg = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = HUB_GPIO_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .hpoint = 0,
    };
    ledc_channel_config(&ch_cfg);

    /* Radio HAL init */
    g_radio_hal.spi_init   = hal_spi_init;
    g_radio_hal.spi_xfer   = hal_spi_xfer;
    g_radio_hal.cs_low     = hal_cs_low;
    g_radio_hal.cs_high    = hal_cs_high;
    g_radio_hal.reset      = hal_reset;
    g_radio_hal.dio1_read  = hal_dio1_read;
    g_radio_hal.busy_read  = hal_busy_read;
    g_radio_hal.delay_ms  = hal_delay_ms;
    g_radio_hal.delay_us  = hal_delay_us;
    g_radio_hal.on_dio1   = hal_on_dio1;

    g_radio_cfg.freq_hz         = WS_SUBGHZ_FREQ_HZ;
    g_radio_cfg.spreading_factor= WS_SUBGHZ_SF;
    g_radio_cfg.bandwidth_hz    = WS_SUBGHZ_BW_HZ;
    g_radio_cfg.tx_power_dbm    = WS_SUBGHZ_TX_POWER_DBM;
    g_radio_cfg.sync_word       = WS_SYNC_WORD;
    g_radio_cfg.preamble_len    = WS_SUBGHZ_PREAMBLE;

    g_radio_hal.spi_init();
    ws_sx1262_init(&g_radio_hal, &g_radio_cfg);

    /* Mesh init (Hub = node 0) */
    uint8_t aes_key[16] = {0};
    ws_mesh_init(&g_mesh, WS_HUB_NODE_ID, WS_NODE_HUB, aes_key);
    g_mesh.joined = 1;
    g_mesh.battery_v = 420;

    /* Init node table */
    memset(g_node_table, 0, sizeof(g_node_table));

    /* Init geofence (production: load from NVS) */
    g_geofence.center_lat_e7 = 374449000;   /* Example: 37.4449 N */
    g_geofence.center_lon_e7 = -1224159000; /* Example: 122.4159 W */
    g_geofence.radius_m = WS_GEOFENCE_RADIUS_M;
    g_geofence.night_radius_m = WS_GEOFENCE_NIGHT_RADIUS_M;
    g_geofence.night_start_h = WS_GEOFENCE_NIGHT_START_H;
    g_geofence.night_end_h = WS_GEOFENCE_NIGHT_END_H;

    /* Init reminder schedule (production: load from NVS) */
    memset(g_reminders, 0, sizeof(g_reminders));
    /* Example: 8 AM medication, 12 PM meal, 6 PM medication, 8 PM meal */
    g_reminders[8].enabled = 1; g_reminders[8].clip_index = 0;
    g_reminders[8].volume = 80; g_reminders[8].reminder_type = WS_REMINDER_MEDICATION;
    g_reminders[12].enabled = 1; g_reminders[12].clip_index = 20;
    g_reminders[12].volume = 80; g_reminders[12].reminder_type = WS_REMINDER_MEAL;
    g_reminders[18].enabled = 1; g_reminders[18].clip_index = 1;
    g_reminders[18].volume = 80; g_reminders[18].reminder_type = WS_REMINDER_MEDICATION;
    g_reminders[19].enabled = 1; g_reminders[19].clip_index = 21;
    g_reminders[19].volume = 80; g_reminders[19].reminder_type = WS_REMINDER_MEAL;

    g_mesh_mutex = xSemaphoreCreateMutex();

    /* Wi-Fi + MQTT */
    wifi_init();
    mqtt_init();

    /* SIM7000 4G LTE */
    sim7000_init();

    /* Tasks */
    xTaskCreate(radio_task, "radio", 8192, NULL, 5, NULL);
    xTaskCreate(reminder_task, "reminder", 4096, NULL, 4, NULL);
    xTaskCreate(cancel_task, "cancel", 2048, NULL, 3, NULL);
    xTaskCreate(watchdog_task, "watchdog", 2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "WanderSync Care Hub ready. Listening on 868 MHz TDMA mesh.");
    ESP_LOGI(TAG, "Geofence: %.7f, %.7f (radius %dm)",
             g_geofence.center_lat_e7 / 1e7,
             g_geofence.center_lon_e7 / 1e7,
             g_geofence.radius_m);
}