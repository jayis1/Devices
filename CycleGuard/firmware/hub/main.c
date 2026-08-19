/*
 * CycleGuard Hub Firmware
 * Target: ESP32-S3-WROOM-1-N8R8
 *
 * Coordinates the CycleGuard system:
 *   - Sub-GHz 868 MHz TDMA mesh coordinator (SX1262) for Smart Lock
 *   - BLE 5.0 central for Smart Helmet, Smart Light, Bike Sensor
 *   - OV5640 front camera + BlindSpotNet inference (tflite-micro)
 *   - CollisionPredict LSTM (3–8 s collision prediction)
 *   - GPS NEO-M9N 10 Hz (speed, heading, route, crash location)
 *   - ILI9341 TFT display (speed, route, proximity warnings)
 *   - MQTT over TLS to cloud backend
 *   - 4G LTE (SIM7600G) crash emergency dispatch with GPS
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
#include "driver/uart.h"
#include "lwip/sockets.h"
#include "mqtt_client.h"

#include "../common/protocol.h"
#include "../common/mesh.h"

static const char *TAG = "CYCLEGUARD_HUB";

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
#define PIN_TFT_DC          11
#define PIN_TFT_RST         12
#define PIN_TFT_BL          13
#define PIN_GPS_TX          27
#define PIN_GPS_RX          28
#define PIN_GPS_PPS         29
#define PIN_I2S_BCLK        30
#define PIN_I2S_LRCK        31
#define PIN_I2S_DIN         32
#define PIN_STATUS_LED      33
#define BTN_LEFT            34
#define BTN_RIGHT           35
#define PIN_BTN_SOS         36
#define PIN_LTE_TX          37
#define PIN_LTE_RX          38
#define PIN_LTE_PWR         39
#define PIN_BAT_SENSE       40
#define PIN_CHG_STAT        41

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

/* ---- GPS state ---- */
typedef struct {
    float    lat;
    float    lon;
    float    speed_kmh;
    float    heading_deg;
    float    altitude_m;
    bool     fix;
    uint32_t fix_time_ms;
} gps_state_t;
static gps_state_t g_gps;

/* ---- Riding state ---- */
static float    g_speed_kmh        = 0.0f;
static float    g_cadence_rpm      = 0.0f;
static float    g_tire_pressure    = 90.0f;
static uint8_t  g_blindspot_class  = BLINDSPOT_CLEAR;
static float    g_collision_prob   = 0.0f;
static uint8_t  g_turn_signal      = TURN_NONE;
static bool     g_crash_detected   = false;
static bool     g_crash_confirmed  = false;
static uint8_t  g_lock_state       = LOCK_DISARMED;
static uint8_t  g_battery_pct      = 100;

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
    spi_bus_initialize(SPI2_HOST, &buscfg, 1);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_SPI_CS_SX1262,
        .queue_size = 7,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_sx1262_spi);
}

/* ---- I2C init ---- */
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
    i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
}

/* ---- TFT display (ILI9341) ---- */
static void tft_init(void)
{
    gpio_set_direction(PIN_TFT_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_TFT_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_TFT_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_TFT_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_TFT_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_TFT_BL, 1);  /* backlight on */
    ESP_LOGI(TAG, "ILI9341 TFT initialized (2.4\" 320x240)");
}

static void tft_render_status(float speed, float cadence, float pressure,
                               uint8_t blindspot, float collision_prob,
                               uint8_t turn, bool crash, uint8_t lock_st)
{
    const char *bs_str = (blindspot == BLINDSPOT_CAR)     ? "CAR!"     :
                         (blindspot == BLINDSPOT_TRUCK)   ? "TRUCK!"   :
                         (blindspot == BLINDSPOT_BUS)     ? "BUS!"     :
                         (blindspot == BLINDSPOT_MOTOR)   ? "MOTORCYCLE" :
                         (blindspot == BLINDSPOT_BICYCLE) ? "Bicycle"  :
                         (blindspot == BLINDSPOT_PED)     ? "Pedestrian" :
                         (blindspot == BLINDSPOT_OBSTACLE)? "OBSTACLE" : "Clear";
    const char *turn_str = (turn == TURN_LEFT)  ? "◄ LEFT"  :
                           (turn == TURN_RIGHT) ? "RIGHT ►" :
                           (turn == TURN_HAZARD)? "HAZARD"  : "";
    const char *lock_str = (lock_st == LOCK_ARMED)    ? "LOCKED"   :
                           (lock_st == LOCK_ALARM)    ? "ALARM!"   :
                           (lock_st == LOCK_TRACKING) ? "TRACKING" : "unlocked";

    ESP_LOGI(TAG, "TFT: %.1f km/h cad=%.0f PSI=%.0f BS=%s coll=%.0f%% turn=%s %s %s",
             speed, cadence, pressure, bs_str, collision_prob * 100,
             turn_str, crash ? "CRASH!" : "", lock_str);

    /* In production: render to ILI9341 frame buffer via SPI */
    /* Colors: speed=white, blindspot=yellow/caution, collision=red,
       crash=blinking red, lock=green/red */
}

/* ---- GPS NEO-M9N parsing ---- */
static void gps_init(void)
{
    /* UART2 for GPS at 38400 baud (NEO-M9N default) */
    uart_config_t cfg = {
        .baud_rate  = 38400,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_NONE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_NUM_2, &cfg);
    uart_set_pin(UART_NUM_2, PIN_GPS_TX, PIN_GPS_RX, -1, -1);
    uart_driver_install(UART_NUM_2, 1024, 0, 0, NULL, 0);
    ESP_LOGI(TAG, "NEO-M9N GPS initialized (UART2, 10 Hz)");
}

static void gps_parse_nmea(const char *sentence)
{
    /* Minimal NMEA parser: $GPRMC + $GPGGA */
    if (strncmp(sentence, "$GPRMC", 6) == 0) {
        /* $GPRMC,datetime,A,lat,N,lon,W,speed,course,date,magvar*CS */
        char status = 0;
        float speed_knots = 0, course = 0, lat = 0, lon = 0;
        /* Simplified parse — production uses full NMEA parser */
        if (sscanf(sentence, "$GPRMC,%*f,%c,%f,%*c,%f,%*c,%f,%f",
                   &status, &lat, &lon, &speed_knots, &course) >= 5) {
            if (status == 'A') {
                g_gps.fix = true;
                g_gps.speed_kmh = speed_knots * 1.852f;  /* knots → km/h */
                g_gps.heading_deg = course;
                g_speed_kmh = g_gps.speed_kmh;
            }
        }
    }
    if (strncmp(sentence, "$GPGGA", 6) == 0) {
        /* $GPGGA,time,lat,N,lon,W,fixquality,sats,hdop,alt,M,... */
        float lat = 0, lon = 0, alt = 0;
        int fix_q = 0;
        if (sscanf(sentence, "$GPGGA,%*f,%f,%*c,%f,%*c,%d,%*d,%*f,%f",
                   &lat, &lon, &fix_q, &alt) >= 4) {
            if (fix_q > 0) {
                g_gps.lat = lat / 100.0f;  /* ddmm.mmmm → dd.mmmm */
                g_gps.lon = lon / 100.0f;
                g_gps.altitude_m = alt;
                g_gps.fix = true;
            }
        }
    }
}

static void gps_task(void *arg)
{
    char buf[256];
    int idx = 0;
    while (1) {
        uint8_t c;
        int len = uart_read_bytes(UART_NUM_2, &c, 1, pdMS_TO_TICKS(100));
        if (len <= 0) continue;
        if (c == '\n') {
            buf[idx] = '\0';
            gps_parse_nmea(buf);
            idx = 0;
        } else if (c != '\r' && idx < (int)sizeof(buf) - 1) {
            buf[idx++] = c;
        }
    }
}

/* ---- SX1262 radio TX/RX ---- */
static void sx1262_transmit(const uint8_t *data, size_t len)
{
    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);
    spi_transaction_t t = {0};
    t.tx_buffer = data;
    t.length = len * 8;
    spi_device_polling_transmit(g_sx1262_spi, &t);
    xSemaphoreGive(g_spi_mutex);
}

static int sx1262_receive(uint8_t *buf, size_t max_len, uint32_t timeout_ms)
{
    /* In production: poll DIO1, read RX buffer, return length */
    return 0;  /* placeholder — real implementation reads SX1262 FIFO */
}

/* ---- MQTT cloud bridge ---- */
static void mqtt_event_handler(void *args, esp_event_base_t base,
                               int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        g_mqtt_connected = true;
        esp_mqtt_client_subscribe(g_mqtt_client, "cycleguard/cmd/#", 1);
        ESP_LOGI(TAG, "MQTT connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        g_mqtt_connected = false;
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT topic: %.*s", event->topic_len, event->topic);
        break;
    default:
        break;
    }
}

static void mqtt_init(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
        .credentials.client_id = "cycleguard-hub",
    };
    g_mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(g_mqtt_client,
                                   (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(g_mqtt_client);
}

static void mqtt_publish_state(void)
{
    if (!g_mqtt_connected) return;
    char payload[512];
    snprintf(payload, sizeof(payload),
        "{\"speed\":%.1f,\"cadence\":%.0f,\"tire_psi\":%.0f,\"bs_class\":%d,"
        "\"collision\":%.2f,\"turn\":%d,\"crash\":%s,\"lock\":%d,"
        "\"gps_lat\":%.6f,\"gps_lon\":%.6f,\"heading\":%.0f,\"battery\":%d}",
        g_speed_kmh, g_cadence_rpm, g_tire_pressure, g_blindspot_class,
        g_collision_prob, g_turn_signal, g_crash_detected ? "true" : "false",
        g_lock_state, g_gps.lat, g_gps.lon, g_gps.heading_deg, g_battery_pct);
    esp_mqtt_client_publish(g_mqtt_client,
                            "cycleguard/hub/state",
                            payload, 0, 1, 0);
}

/* ---- 4G LTE emergency dispatch (SIM7600G) ---- */
static void lte_init(void)
{
    gpio_set_direction(PIN_LTE_PWR, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LTE_PWR, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(PIN_LTE_PWR, 1);
    ESP_LOGI(TAG, "SIM7600G 4G LTE initialized (emergency backup)");
}

static void lte_send_emergency(const char *message, float lat, float lon)
{
    /* AT commands: AT+CMGF=1 (SMS text mode), AT+CMGS="911" */
    char at_cmd[256];
    char msg[256];
    snprintf(msg, sizeof(msg), "%s LAT:%.6f LON:%.6f", message, lat, lon);
    snprintf(at_cmd, sizeof(at_cmd), "AT+CMGS=\"+18055551234\"");
    ESP_LOGW(TAG, "LTE 911 DISPATCH: %s", msg);
    /* In production: send via UART to SIM7600G */
}

/* ---- Crash confirmation + dispatch ---- */
static void crash_confirmation_task(void *arg)
{
    uint8_t alert;
    while (1) {
        if (xQueueReceive(g_alert_queue, &alert, portMAX_DELAY)) {
            if (alert == 0x01) {  /* crash alert from helmet */
                g_crash_detected = true;
                ESP_LOGW(TAG, "CRASH ALERT from helmet — starting 10s confirmation");

                /* 10-second confirmation window (cancelable via SOS long-press) */
                bool cancelled = false;
                for (int i = 0; i < 10; i++) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    /* Check if rider cancelled (button press) */
                    if (gpio_get_level(PIN_BTN_SOS) == 0) {
                        /* Long press detection simplified */
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        if (gpio_get_level(PIN_BTN_SOS) == 0) {
                            cancelled = true;
                            ESP_LOGI(TAG, "Crash alert cancelled by rider");
                            break;
                        }
                    }
                    /* If rider still moving (GPS speed > 5 km/h), likely false alarm */
                    if (g_gps.fix && g_gps.speed_kmh > 5.0f && i > 3) {
                        cancelled = true;
                        ESP_LOGI(TAG, "Crash alert auto-cancelled (rider still moving)");
                        break;
                    }
                }

                if (!cancelled) {
                    g_crash_confirmed = true;
                    ESP_LOGE(TAG, "CRASH CONFIRMED — dispatching 911 + emergency contact");

                    /* Dispatch 911 via 4G LTE with GPS */
                    lte_send_emergency("Cyclist crash detected — CycleGuard emergency",
                                       g_gps.lat, g_gps.lon);

                    /* Activate hazard lights via BLE to Smart Light */
                    light_cmd_payload_t cmd = {0};
                    cmd.mode = 2;  /* crash mode */
                    mesh_frame_t frame;
                    protocol_build_frame(&frame, g_mesh.self_id, NODE_ID_SMART_LIGHT,
                                         MSG_TYPE_LIGHT_CMD, g_mesh.seq_counter++,
                                         (uint8_t *)&cmd, sizeof(cmd));
                    /* ble_write(NODE_ID_SMART_LIGHT, &frame, sizeof(frame)); */

                    /* Publish crash event to cloud */
                    if (g_mqtt_connected) {
                        char crash_payload[256];
                        snprintf(crash_payload, sizeof(crash_payload),
                            "{\"crash\":true,\"lat\":%.6f,\"lon\":%.6f,\"speed\":%.1f}",
                            g_gps.lat, g_gps.lon, g_speed_kmh);
                        esp_mqtt_client_publish(g_mqtt_client,
                            "cycleguard/alert/crash", crash_payload, 0, 1, 0);
                    }
                }
                g_crash_detected = false;
            }
        }
    }
}

/* ---- CollisionPredict (edge ML) ---- */
static void collision_predict_task(void *arg)
{
    /* Runs every 500 ms: fuses GPS speed, BlindSpotNet detections,
       wheel speed, heading changes to predict collision 3–8 s ahead */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));

        /* Heuristic collision probability (production: LSTM model) */
        float prob = 0.0f;

        /* High risk if vehicle approaching from behind + high speed */
        if (g_blindspot_class == BLINDSPOT_CAR ||
            g_blindspot_class == BLINDSPOT_TRUCK ||
            g_blindspot_class == BLINDSPOT_BUS) {
            prob += 0.3f;
        }

        /* Higher risk at intersections (speed changes + turning) */
        if (g_speed_kmh > 0 && g_gps.fix) {
            /* Heading change rate would be computed from GPS history */
            /* Simplified: if turning, increase risk */
            if (g_turn_signal != TURN_NONE) prob += 0.2f;
        }

        /* High speed increases risk */
        if (g_speed_kmh > 30.0f) prob += 0.1f;
        if (g_speed_kmh > 45.0f) prob += 0.15f;

        g_collision_prob = prob > 1.0f ? 1.0f : prob;

        /* Trigger haptic warning if collision probability > 0.6 */
        if (g_collision_prob > 0.6f && !g_crash_detected) {
            ESP_LOGW(TAG, "COLLISION RISK %.0f%% — haptic triple-burst to helmet",
                     g_collision_prob * 100);
            /* Send proximity warning to helmet via BLE */
            uint8_t warn_payload = HAPTIC_TRIPLE_BURST;
            mesh_frame_t frame;
            protocol_build_frame(&frame, g_mesh.self_id, NODE_ID_SMART_HELMET,
                                 MSG_TYPE_PROXIMITY_WARN, g_mesh.seq_counter++,
                                 &warn_payload, 1);
            /* ble_notify(NODE_ID_SMART_HELMET, &frame, sizeof(frame)); */
        }
    }
}

/* ---- TDMA mesh coordinator task (Sub-GHz for Smart Lock) ---- */
static void mesh_coordinator_task(void *arg)
{
    mesh_frame_t beacon;
    uint8_t rx_buf[FRAME_MAX_LEN];

    while (1) {
        uint32_t now = esp_timer_get_time() / 1000;
        g_mesh.superframe_start_ms = now;

        /* Broadcast beacon */
        mesh_build_beacon(&g_mesh, &beacon);
        sx1262_transmit((uint8_t *)&beacon, FRAME_MAX_LEN);

        /* Listen for node frames in their slots */
        for (int slot = 0; slot < TDMAX_SLOTS; slot++) {
            int len = sx1262_receive(rx_buf, sizeof(rx_buf), TDMA_SLOT_MS);
            if (len == FRAME_MAX_LEN) {
                mesh_frame_t frame;
                if (protocol_parse_frame(rx_buf, len, &frame)) {
                    mesh_process_frame(&g_mesh, &frame, now);

                    if (frame.msg_type == MSG_TYPE_SENSOR_DATA &&
                        frame.src_id == NODE_ID_SMART_LOCK) {
                        lock_payload_t lp;
                        memcpy(&lp, frame.payload, sizeof(lp));
                        g_lock_state = lp.lock_state;
                        ESP_LOGI(TAG, "Lock: state=%d GPS=%.6f,%.6f battery=%d",
                                 lp.lock_state,
                                 lp.gps_lat_e7 / 1e7f,
                                 lp.gps_lon_e7 / 1e7f,
                                 lp.battery_pct);

                        if (g_mqtt_connected) {
                            char topic[64];
                            snprintf(topic, sizeof(topic),
                                     "cycleguard/lock/%04x/data", frame.src_id);
                            esp_mqtt_client_publish(g_mqtt_client, topic,
                                (char *)frame.payload, frame.length, 1, 0);
                        }
                    }

                    if (frame.msg_type == MSG_TYPE_THEFT_ALERT) {
                        ESP_LOGE(TAG, "THEFT ALERT from Smart Lock!");
                        if (g_mqtt_connected) {
                            esp_mqtt_client_publish(g_mqtt_client,
                                "cycleguard/alert/theft",
                                (char *)frame.payload, frame.length, 1, 0);
                        }
                    }
                }
            }
        }
        mesh_remove_stale(&g_mesh, now);
    }
}

/* ---- Display update task (every 500 ms for responsive UI) ---- */
static void display_task(void *arg)
{
    while (1) {
        tft_render_status(g_speed_kmh, g_cadence_rpm, g_tire_pressure,
                          g_blindspot_class, g_collision_prob,
                          g_turn_signal, g_crash_confirmed, g_lock_state);
        mqtt_publish_state();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ---- BLE notification handlers (from Helmet, Light, Bike Sensor) ---- */
static void ble_notify_helmet(const uint8_t *data, size_t len)
{
    if (len < sizeof(helmet_payload_t)) return;
    helmet_payload_t hp;
    memcpy(&hp, data, sizeof(hp));

    ESP_LOGI(TAG, "BLE Helmet: crash=%d impact=%.1fg rot=%.0f°/s horn=%d siren=%d",
             hp.crash_class, hp.impact_g, hp.rot_velocity,
             hp.horn_detected, hp.siren_detected);

    if (hp.crash_class == CRASH_CRASH) {
        uint8_t alert = 0x01;
        xQueueSend(g_alert_queue, &alert, 0);
    }

    /* Horn/siren detected → warn rider via bone conduction */
    if (hp.horn_detected || hp.siren_detected) {
        ESP_LOGI(TAG, "Horn/siren detected — audio alert to rider");
    }
}

static void ble_notify_bike_sensor(const uint8_t *data, size_t len)
{
    if (len < sizeof(bike_sensor_payload_t)) return;
    bike_sensor_payload_t bp;
    memcpy(&bp, data, sizeof(bp));

    g_speed_kmh = bp.speed_kmh;
    g_cadence_rpm = bp.cadence_rpm;
    g_tire_pressure = bp.tire_pressure_psi;

    /* Low tire pressure alert */
    if (bp.tire_pressure_psi < 60.0f && bp.tire_pressure_psi > 0) {
        ESP_LOGW(TAG, "LOW TIRE PRESSURE: %.0f PSI", bp.tire_pressure_psi);
    }
}

static void ble_notify_light(const uint8_t *data, size_t len)
{
    if (len < sizeof(light_payload_t)) return;
    light_payload_t lp;
    memcpy(&lp, data, sizeof(lp));
    /* Light status update — braking, brightness, etc. */
}

/* ---- Turn signal button handlers ---- */
static void IRAM_ATTR btn_left_handler(void *arg)
{
    g_turn_signal = (g_turn_signal == TURN_LEFT) ? TURN_NONE : TURN_LEFT;
    ESP_LOGI(TAG, "Turn signal: %s",
             g_turn_signal == TURN_LEFT ? "LEFT" : "OFF");

    /* Send turn signal command to Smart Light via BLE */
    light_cmd_payload_t cmd = {0};
    cmd.turn_signal = g_turn_signal;
    mesh_frame_t frame;
    protocol_build_frame(&frame, g_mesh.self_id, NODE_ID_SMART_LIGHT,
                         MSG_TYPE_LIGHT_CMD, g_mesh.seq_counter++,
                         (uint8_t *)&cmd, sizeof(cmd));
    /* ble_write(NODE_ID_SMART_LIGHT, &frame, sizeof(frame)); */
}

static void IRAM_ATTR btn_right_handler(void *arg)
{
    g_turn_signal = (g_turn_signal == TURN_RIGHT) ? TURN_NONE : TURN_RIGHT;
    ESP_LOGI(TAG, "Turn signal: %s",
             g_turn_signal == TURN_RIGHT ? "RIGHT" : "OFF");

    light_cmd_payload_t cmd = {0};
    cmd.turn_signal = g_turn_signal;
    mesh_frame_t frame;
    protocol_build_frame(&frame, g_mesh.self_id, NODE_ID_SMART_LIGHT,
                         MSG_TYPE_LIGHT_CMD, g_mesh.seq_counter++,
                         (uint8_t *)&cmd, sizeof(cmd));
    /* ble_write(NODE_ID_SMART_LIGHT, &frame, sizeof(frame)); */
}

static void IRAM_ATTR btn_sos_handler(void *arg)
{
    /* SOS button — if crash alert active, cancels it; otherwise sends SOS */
    if (g_crash_detected) {
        /* Cancel handled in confirmation task by checking button state */
        ESP_LOGI(TAG, "SOS button — crash cancel requested");
    } else {
        /* Manual SOS — send emergency alert */
        uint8_t alert = 0x01;
        xQueueSend(g_alert_queue, &alert, 0);
        ESP_LOGW(TAG, "SOS button pressed — manual emergency");
    }
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "CycleGuard Hub starting...");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* GPIO */
    gpio_set_direction(PIN_STATUS_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BTN_SOS, GPIO_MODE_INPUT);
    gpio_set_direction(BTN_LEFT, GPIO_MODE_INPUT);
    gpio_set_direction(BTN_RIGHT, GPIO_MODE_INPUT);
    gpio_pullup_en(PIN_BTN_SOS);
    gpio_pullup_en(BTN_LEFT);
    gpio_pullup_en(BTN_RIGHT);
    gpio_set_intr_type(PIN_BTN_SOS, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(BTN_LEFT, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(BTN_RIGHT, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_LEFT, btn_left_handler, NULL);
    gpio_isr_handler_add(BTN_RIGHT, btn_right_handler, NULL);
    gpio_isr_handler_add(PIN_BTN_SOS, btn_sos_handler, NULL);

    /* Queues */
    g_sensor_queue = xQueueCreate(32, sizeof(mesh_frame_t));
    g_alert_queue  = xQueueCreate(8, sizeof(uint8_t));
    g_spi_mutex    = xSemaphoreCreateMutex();

    /* Peripherals */
    i2c_init();
    sx1262_spi_init();
    tft_init();
    gps_init();
    lte_init();

    /* Mesh */
    mesh_init(&g_mesh, NODE_ID_HUB, true);

    /* Wi-Fi + MQTT */
    esp_netif_init();
    esp_event_loop_create_default();
    mqtt_init();

    /* Tasks */
    xTaskCreate(gps_task, "gps", 4096, NULL, 4, NULL);
    xTaskCreate(mesh_coordinator_task, "mesh_coord", 8192, NULL, 5, NULL);
    xTaskCreate(display_task, "display", 4096, NULL, 3, NULL);
    xTaskCreate(crash_confirmation_task, "crash_conf", 4096, NULL, 6, NULL);
    xTaskCreate(collision_predict_task, "collision", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "CycleGuard Hub ready. Nodes: Smart Helmet, Smart Light, "
                   "Bike Sensor (BLE); Smart Lock (Sub-GHz).");

    /* Demo: trigger test proximity warning after 30 s */
    vTaskDelay(pdMS_TO_TICKS(30000));
    ESP_LOGI(TAG, "Demo: simulating vehicle behind (BlindSpotNet)");
    g_blindspot_class = BLINDSPOT_CAR;
}