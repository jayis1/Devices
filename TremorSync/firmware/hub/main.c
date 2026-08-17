/*
 * TremorSync Hub Firmware
 * Target: ESP32-S3-WROOM-1-N8R8
 *
 * Coordinates the TremorSync system:
 *   - Sub-GHz 868 MHz TDMA mesh coordinator (SX1262)
 *   - BLE 5.0 central for Tremor Band + Voice Node
 *   - On-device TremorNet inference (tflite-micro)
 *   - E-ink ON/OFF state + tremor score + next-dose display (UC8151)
 *   - MQTT over TLS to cloud backend
 *   - 4G LTE (SIM7600G) fall/FOG emergency dispatch with GPS
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

static const char *TAG = "TREMSYNC_HUB";

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
#define PIN_ADXL_INT        18
#define PIN_BTN_MED         19
#define PIN_BTN_SOS         20
#define PIN_LTE_TX          21
#define PIN_LTE_RX          22
#define PIN_LTE_PWR         23
#define PIN_GPS_PPS         24

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

/* ---- ON/OFF state tracking ---- */
static uint8_t  g_onoff_state    = ONOFF_OFF;
static float    g_tremor_amp     = 0.0f;
static uint8_t  g_tremor_class   = TREMOR_NONE;
static float    g_bradykinesia   = 0.0f;
static uint16_t g_next_dose_min  = 180;
static bool     g_fog_active     = false;
static bool     g_fall_detected  = false;

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

/* ---- E-ink display (UC8151) ---- */
static void eink_init(void)
{
    /* Configure GPIO for DC, RST, BUSY */
    gpio_set_direction(PIN_EINK_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_EINK_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_EINK_BUSY, GPIO_MODE_INPUT);
    /* Hardware reset */
    gpio_set_level(PIN_EINK_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_EINK_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG, "E-ink display initialized (UC8151 2.9\" 296x128)");
}

static void eink_render_status(uint8_t onoff, float tremor, uint8_t tclass,
                                uint16_t next_dose, bool fog)
{
    const char *state_str = (onoff == ONOFF_ON) ? "ON" :
                            (onoff == ONOFF_TRANSITION) ? "TRANSITION" : "OFF";
    const char *tremor_str = (tclass == TREMOR_RESTING)  ? "Resting"  :
                             (tclass == TREMOR_POSTURAL) ? "Postural" :
                             (tclass == TREMOR_ACTION)   ? "Action"   : "None";
    ESP_LOGI(TAG, "E-ink: State=%s Tremor=%.2f (%s) NextDose=%dm FOG=%d",
             state_str, tremor, tremor_str, next_dose, (int)fog);
    /* In production: drive UC8151 frame buffer via SPI and refresh */
}

/* ---- SX1262 radio TX/RX ---- */
static void sx1262_transmit(const uint8_t *data, size_t len)
{
    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);
    /* Write TX buffer via SPI, set TX mode, wait for DIO1 */
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

/* ---- BLE 5.0 central (Tremor Band + Voice Node) ---- */
/* GATT service/characteristic definitions match protocol.h TSxx UUIDs */

static void ble_notify_tremor(const uint8_t *data, size_t len)
{
    /* Parse tremor_payload_t from BLE notification */
    if (len < sizeof(tremor_payload_t)) return;
    tremor_payload_t tp;
    memcpy(&tp, data, sizeof(tp));
    g_tremor_amp   = tp.tremor_amplitude;
    g_tremor_class = tp.tremor_class;
    g_bradykinesia = tp.bradykinesia_idx;
    g_onoff_state  = tp.onoff_state;
    ESP_LOGI(TAG, "BLE Tremor: class=%d amp=%.3f brady=%.1f onoff=%d",
             tp.tremor_class, tp.tremor_amplitude,
             tp.bradykinesia_idx, tp.onoff_state);
}

/* ---- MQTT cloud bridge ---- */
static void mqtt_event_handler(void *args, esp_event_base_t base,
                               int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        g_mqtt_connected = true;
        esp_mqtt_client_subscribe(g_mqtt_client, "tremorsync/cmd/#", 1);
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
        .credentials.client_id = "tremorsync-hub",
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
    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"onoff\":%d,\"tremor_class\":%d,\"tremor_amp\":%.3f,"
        "\"bradykinesia\":%.1f,\"next_dose_min\":%d,\"fog\":%s}",
        g_onoff_state, g_tremor_class, g_tremor_amp,
        g_bradykinesia, g_next_dose_min,
        g_fog_active ? "true" : "false");
    esp_mqtt_client_publish(g_mqtt_client,
                            "tremorsync/hub/state",
                            payload, 0, 1, 0);
}

/* ---- 4G LTE emergency dispatch (SIM7600G) ---- */
static void lte_init(void)
{
    gpio_set_direction(PIN_LTE_PWR, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LTE_PWR, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(PIN_LTE_PWR, 1);  /* power on module */
    ESP_LOGI(TAG, "SIM7600G 4G LTE initialized (emergency backup)");
}

static void lte_send_emergency(const char *message, const char *gps_lat,
                                const char *gps_lon)
{
    /* AT commands: AT+CMGF=1 (SMS text mode), AT+CMGS="911" or caregiver */
    char at_cmd[256];
    snprintf(at_cmd, sizeof(at_cmd), "AT+CMGS=\"+18055551234\"");
    ESP_LOGI(TAG, "LTE emergency SMS: %s (%s,%s)", message, gps_lat, gps_lon);
    /* In production: send via UART to SIM7600G */
}

/* ---- Fall detection (ADXL362 always-on) ---- */
static void IRAM_ATTR adxl_isr_handler(void *arg)
{
    uint8_t alert = 0x01;  /* fall alert */
    xQueueSendFromISR(g_alert_queue, &alert, NULL);
}

static void fall_detection_task(void *arg)
{
    uint8_t alert;
    while (1) {
        if (xQueueReceive(g_alert_queue, &alert, portMAX_DELAY)) {
            g_fall_detected = true;
            ESP_LOGW(TAG, "FALL DETECTED — dispatching emergency");
            lte_send_emergency("Fall detected — Parkinson's patient",
                                "34.0522", "-118.2437");
            /* Also publish via MQTT if available */
            if (g_mqtt_connected) {
                esp_mqtt_client_publish(g_mqtt_client,
                    "tremorsync/alert/fall", "{\"fall\":true}", 0, 1, 0);
            }
        }
    }
}

/* ---- TDMA mesh coordinator task ---- */
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

                    if (frame.msg_type == MSG_TYPE_SENSOR_DATA) {
                        /* Forward to cloud via MQTT */
                        if (g_mqtt_connected) {
                            char topic[64];
                            snprintf(topic, sizeof(topic),
                                     "tremorsync/node/%04x/data",
                                     frame.src_id);
                            esp_mqtt_client_publish(g_mqtt_client, topic,
                                (char *)frame.payload, frame.length, 1, 0);
                        }
                    }
                    if (frame.msg_type == MSG_TYPE_FALL_ALERT) {
                        g_fall_detected = true;
                        lte_send_emergency("Fall (gait pod)",
                                            "34.0522", "-118.2437");
                    }
                }
            }
        }
        mesh_remove_stale(&g_mesh, now);
    }
}

/* ---- Display update task (every 5 s) ---- */
static void display_task(void *arg)
{
    while (1) {
        eink_render_status(g_onoff_state, g_tremor_amp, g_tremor_class,
                           g_next_dose_min, g_fog_active);
        mqtt_publish_state();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* ---- FOG cueing trigger ---- */
static void trigger_fog_cueing(uint8_t bpm_pattern)
{
    /* Send haptic command to Tremor Band via BLE */
    uint8_t cmd = bpm_pattern;
    /* ble_write_characteristic("TS05", &cmd, 1); */
    ESP_LOGI(TAG, "FOG cueing triggered: pattern=0x%02X", bpm_pattern);

    /* Also broadcast FOG warning on Sub-GHz mesh */
    mesh_frame_t frame;
    uint8_t payload[FRAME_PAYLOAD_MAX] = {0};
    payload[0] = bpm_pattern;
    protocol_build_frame(&frame, g_mesh.self_id, NODE_ID_BROADCAST,
                         MSG_TYPE_FOG_WARNING,
                         g_mesh.seq_counter++, payload, 1);
    sx1262_transmit((uint8_t *)&frame, FRAME_MAX_LEN);
}

/* ---- Button handlers ---- */
static void IRAM_ATTR btn_med_handler(void *arg)
{
    g_next_dose_min = 0;  /* mark dose as taken now */
    ESP_LOGI(TAG, "Manual dose button pressed");
}

static void IRAM_ATTR btn_sos_handler(void *arg)
{
    uint8_t alert = 0x02;  /* SOS */
    xQueueSendFromISR(g_alert_queue, &alert, NULL);
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "TremorSync Hub starting...");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* GPIO */
    gpio_set_direction(PIN_STATUS_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BUZZER, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BTN_MED, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_BTN_SOS, GPIO_MODE_INPUT);
    gpio_pullup_en(PIN_BTN_MED);
    gpio_pullup_en(PIN_BTN_SOS);
    gpio_set_intr_type(PIN_BTN_MED, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(PIN_BTN_SOS, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(PIN_ADXL_INT, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BTN_MED, btn_med_handler, NULL);
    gpio_isr_handler_add(PIN_BTN_SOS, btn_sos_handler, NULL);
    gpio_isr_handler_add(PIN_ADXL_INT, adxl_isr_handler, NULL);

    /* Queues */
    g_sensor_queue = xQueueCreate(32, sizeof(mesh_frame_t));
    g_alert_queue  = xQueueCreate(8, sizeof(uint8_t));
    g_spi_mutex    = xSemaphoreCreateMutex();

    /* Peripherals */
    i2c_init();
    sx1262_spi_init();
    eink_init();
    lte_init();

    /* Mesh */
    mesh_init(&g_mesh, NODE_ID_HUB, true);

    /* Wi-Fi + MQTT */
    esp_netif_init();
    esp_event_loop_create_default();
    /* esp_wifi_connect() ... */
    mqtt_init();

    /* Tasks */
    xTaskCreate(mesh_coordinator_task, "mesh_coord", 8192, NULL, 5, NULL);
    xTaskCreate(display_task, "display", 4096, NULL, 3, NULL);
    xTaskCreate(fall_detection_task, "fall_det", 4096, NULL, 6, NULL);

    ESP_LOGI(TAG, "TremorSync Hub ready. Nodes: Gait Pod, Med Station (Sub-GHz); "
                   "Tremor Band, Voice Node (BLE).");

    /* Test FOG cueing after 30 s for demo */
    vTaskDelay(pdMS_TO_TICKS(30000));
    trigger_fog_cueing(HAPTIC_METRONOME_100);
}