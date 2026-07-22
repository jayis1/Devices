/*
 * VoiceSync — Humidity Node Firmware (Smart Humidifier Controller)
 * ESP32-WROOM-32E, ESP-IDF v5.x, FreeRTOS
 *
 * The Humidity Node monitors room humidity via SHT40, controls a
 * smart humidifier relay to maintain 40-60% RH (optimal for vocal
 * cord health), monitors water tank level via ultrasonic sensor,
 * and reports to Hub every 5 minutes via Sub-GHz 868 MHz mesh.
 *
 * Dry air (RH <40%) desiccates vocal cords, reducing vocal fold
 * viscoelasticity and increasing phonation threshold pressure.
 * Maintaining 40-60% RH is the #1 environmental factor for voice health.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "VoiceSync-Humidity";

/* === Sensor state === */
static float g_temp_c = 0.0f;
static float g_humidity_pct = 0.0f;
static uint8_t g_tank_level_pct = 100;
static uint8_t g_humidifier_on = 0;
static uint8_t g_fan_on = 0;

/* === Mesh state === */
static vs_mesh_ctx_t g_mesh;
static uint16_t g_msg_seq = 0;

/* === SX1262 SPI Interface (ESP32) === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = HUM_GPIO_SX_MOSI,
        .miso_io_num = HUM_GPIO_SX_MISO,
        .sclk_io_num = HUM_GPIO_SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(VSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = HUM_GPIO_SX_NSS,
        .queue_size = 4,
    };
    spi_bus_add_device(VSPI_HOST, &devcfg, &g_spi_dev);
}

static void spi_cs_select(void) { }
static void spi_cs_release(void) { }
static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_transaction_t t = { .tx_buffer = &byte, .rx_buffer = &rx, .length = 8 };
    spi_device_polling_transmit(g_spi_dev, &t);
    return rx;
}
static void spi_reset(uint8_t assert) {
    gpio_set_level(HUM_GPIO_SX_RST, assert ? 0 : 1);
}
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(HUM_GPIO_SX_DIO1); }
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

/* === I²C for SHT40 === */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = HUM_GPIO_SHT_SDA,
        .scl_io_num = HUM_GPIO_SHT_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

static void read_sht40(float *temp, float *humidity)
{
    uint8_t cmd = 0xFD;
    i2c_cmd_handle_t h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, 0x44 << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(h, cmd, true);
    i2c_master_stop(h);
    i2c_master_cmd_begin(I2C_NUM_0, h, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(h);

    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t buf[6];
    h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, 0x44 << 1 | I2C_MASTER_READ, true);
    i2c_master_read(h, buf, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(h);
    i2c_master_cmd_begin(I2C_NUM_0, h, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(h);

    uint16_t t_raw = (buf[0] << 8) | buf[1];
    uint16_t h_raw = (buf[3] << 8) | buf[4];
    *temp = -45.0f + 175.0f * t_raw / 65535.0f;
    *humidity = 100.0f * h_raw / 65535.0f;
}

/* === Ultrasonic Tank Level (HC-SR04) === */
static uint8_t read_tank_level(void)
{
    /* In production:
     * 1. Send 10µs pulse on TRIG pin
     * 2. Measure echo pulse width on ECHO pin
     * 3. Distance = pulse_width × 0.0343 / 2 cm
     * 4. Tank height known (e.g. 20 cm), level = (1 - distance/20) × 100
     */
    return 85; /* Stub: 85% full */
}

/* === Humidifier Control === */
static void humidifier_on(void)
{
    if (g_tank_level_pct < TANK_EMPTY_PCT) {
        ESP_LOGW(TAG, "Cannot start humidifier: tank empty!");
        return;
    }
    gpio_set_level(HUM_GPIO_HUM_RELAY, 1);
    g_humidifier_on = 1;
    ESP_LOGI(TAG, "Humidifier ON (humidity: %.1f%%)", g_humidity_pct);
}

static void humidifier_off(void)
{
    gpio_set_level(HUM_GPIO_HUM_RELAY, 0);
    g_humidifier_on = 0;
    ESP_LOGI(TAG, "Humidifier OFF (humidity: %.1f%%)", g_humidity_pct);
}

static void fan_on(void)
{
    gpio_set_level(HUM_GPIO_FAN_RELAY, 1);
    g_fan_on = 1;
}

static void fan_off(void)
{
    gpio_set_level(HUM_GPIO_FAN_RELAY, 0);
    g_fan_on = 0;
}

/* === Humidity Control Task ===
 * Hysteresis control: ON when humidity < target_min, OFF when > target_max
 * Fan (excess humidity removal): ON when humidity > 65%
 */
static void humidity_control_task(void *arg)
{
    while (1) {
        read_sht40(&g_temp_c, &g_humidity_pct);
        g_tank_level_pct = read_tank_level();

        ESP_LOGI(TAG, "Temp: %.1f°C, Humidity: %.1f%%, Tank: %d%%, Hum: %s",
                 g_temp_c, g_humidity_pct, g_tank_level_pct,
                 g_humidifier_on ? "ON" : "OFF");

        /* Humidifier hysteresis control */
        if (g_humidity_pct < HUMIDITY_TARGET_MIN && !g_humidifier_on) {
            humidifier_on();
        } else if (g_humidity_pct > HUMIDITY_TARGET_MAX && g_humidifier_on) {
            humidifier_off();
        }

        /* Fan control for excess humidity */
        if (g_humidity_pct > 65.0f && !g_fan_on) {
            fan_on();
        } else if (g_humidity_pct < 55.0f && g_fan_on) {
            fan_off();
        }

        /* Tank empty alert */
        if (g_tank_level_pct < TANK_EMPTY_PCT && g_humidifier_on) {
            humidifier_off();
            vs_message_t alert;
            vs_build_alert(&alert, g_mesh.node_id, g_msg_seq++,
                           VS_ALERT_TANK_EMPTY, 1, NULL, 0);
            uint8_t buf[VS_MAX_MSG];
            size_t len = vs_encode(&alert, buf, sizeof(buf));
            vs_mesh_send(&g_mesh, &alert);
            ESP_LOGW(TAG, "Tank empty! Humidifier stopped.");
        }

        vTaskDelay(pdMS_TO_TICKS(HUMIDITY_SAMPLE_MS));
    }
}

/* === Telemetry TX Task === */
static void telemetry_task(void *arg)
{
    while (1) {
        if (g_mesh.joined) {
            vs_message_t msg;
            vs_build_humidity_telem(&msg, g_mesh.node_id, g_msg_seq++,
                                     0xFF, /* USB powered */
                                     (int16_t)(g_temp_c * 10.0f),
                                     (uint16_t)(g_humidity_pct * 10.0f),
                                     g_tank_level_pct,
                                     g_humidifier_on,
                                     g_fan_on,
                                     g_mesh.last_rssi);

            vs_mesh_send(&g_mesh, &msg);
            ESP_LOGI(TAG, "Telemetry sent");
        }
        vTaskDelay(pdMS_TO_TICKS(HUMIDITY_TX_MS));
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "VoiceSync Humidity Node starting...");

    /* Initialize GPIO for relays */
    gpio_set_direction(HUM_GPIO_HUM_RELAY, GPIO_MODE_OUTPUT);
    gpio_set_level(HUM_GPIO_HUM_RELAY, 0);
    gpio_set_direction(HUM_GPIO_FAN_RELAY, GPIO_MODE_OUTPUT);
    gpio_set_level(HUM_GPIO_FAN_RELAY, 0);

    /* Initialize I²C */
    i2c_init();

    /* Initialize SPI for SX1262 */
    spi_init();

    /* Join mesh network */
    vs_radio_config_t radio_cfg = {
        .frequency = VS_NET_FREQ_HZ,
        .bandwidth = VS_NET_BW_HZ,
        .spreading_factor = VS_NET_SF,
        .coding_rate = VS_NET_CR,
        .preamble_len = VS_NET_PREAMBLE,
        .tx_power_dbm = VS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    if (vs_mesh_init(&g_mesh, VS_NODE_HUMIDITY, &g_spi_iface, &radio_cfg) != 0) {
        ESP_LOGE(TAG, "Mesh init failed");
        return;
    }

    for (int i = 0; i < 10; i++) {
        if (vs_mesh_join(&g_mesh) == 0) {
            ESP_LOGI(TAG, "Joined mesh: id=%d slot=%d",
                     g_mesh.node_id, g_mesh.tdma_slot);
            break;
        }
        ESP_LOGW(TAG, "Join attempt %d failed, retrying...", i + 1);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    /* Start tasks */
    xTaskCreate(humidity_control_task, "hum_ctrl", 4096, NULL, 5, NULL);
    xTaskCreate(telemetry_task, "telemetry", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "VoiceSync Humidity Node running");
}