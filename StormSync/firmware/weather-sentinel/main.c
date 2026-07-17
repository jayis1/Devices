/*
 * StormSync — Weather Sentinel Firmware
 * ESP32-S3, FreeRTOS
 *
 * Monitors rain accumulation, wind speed/direction, barometric pressure
 * trend, temperature, and humidity. Transmits every 5 min via Sub-GHz mesh.
 * Solar-powered with LiFePO4 battery.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/pcnt.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "StormSync-Wx";

static ss_mesh_ctx_t g_mesh;
static uint16_t g_msg_seq = 0;

/* Wind/rain counters (ISR) */
static volatile uint16_t g_wind_pulses = 0;
static volatile uint16_t g_rain_tips = 0;

/* Pressure history for trend analysis */
#define PRESSURE_HISTORY_LEN 36  /* 3 hours at 5-min intervals */
static uint16_t g_pressure_history[PRESSURE_HISTORY_LEN];
static int g_pressure_idx = 0;

/* === SX1262 SPI === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = WX_GPIO_SX_MOSI,
        .miso_io_num = WX_GPIO_SX_MISO,
        .sclk_io_num = WX_GPIO_SX_SCK,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000, .mode = 0,
        .spics_io_num = WX_GPIO_SX_NSS, .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi_dev);
}

static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_transaction_t t = { .tx_buffer = &byte, .rx_buffer = &rx, .length = 8 };
    spi_device_polling_transmit(g_spi_dev, &t);
    return rx;
}
static void spi_reset(uint8_t assert) { gpio_set_level(WX_GPIO_SX_RST, assert ? 0 : 1); }
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(WX_GPIO_SX_DIO1); }

static const ss_spi_interface_t g_spi_iface = {
    .init = spi_init, .cs_select = NULL, .cs_release = NULL,
    .transfer = spi_transfer, .reset = spi_reset,
    .delay_ms = spi_delay_ms, .dio1_read = spi_dio1_read,
    .dio1_irq_enable = NULL,
};

/* === I²C for BME280 === */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = WX_GPIO_BME_SDA,
        .scl_io_num = WX_GPIO_BME_SCL,
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

/* === Wind Speed (pulse counter) === */
static void IRAM_ATTR wind_isr(void *arg)
{
    g_wind_pulses++;
}

/* === Rain Gauge (tipping bucket) === */
static void IRAM_ATTR rain_isr(void *arg)
{
    g_rain_tips++;
}

/* === Wind Direction (potentiometer) === */
static uint16_t read_wind_dir(void)
{
    /* Davis 6410 vane: 0-3.3V → 0-360 degrees */
    /* Read via ADC on GPIO9 */
    /* Simplified: return 180 (south) */
    return 180;
}

/* === Pressure Trend Analysis === */
static uint8_t compute_pressure_trend(void)
{
    if (g_pressure_idx < PRESSURE_HISTORY_LEN)
        return 0; /* Not enough data */

    /* Compare 3-hour average to 24-hour (if available) */
    float recent = 0, older = 0;
    int recent_count = 0, older_count = 0;

    for (int i = 0; i < PRESSURE_HISTORY_LEN; i++) {
        if (i < 12) { /* Last hour */
            recent += g_pressure_history[i];
            recent_count++;
        } else {
            older += g_pressure_history[i];
            older_count++;
        }
    }

    recent /= recent_count;
    older /= older_count;
    float delta = recent - older;

    if (delta > 1.0) return 1;  /* Rising */
    if (delta < -1.0) return 2; /* Falling — storm indicator! */
    return 0; /* Steady */
}

/* === Sensor Task === */
static void sensor_task(void *arg)
{
    while (1) {
        float temp, hum, pres;
        read_bme280(&temp, &hum, &pres);

        /* Store pressure for trend analysis */
        g_pressure_history[g_pressure_idx % PRESSURE_HISTORY_LEN] =
            (uint16_t)(pres * 10);
        g_pressure_idx++;

        uint8_t trend = compute_pressure_trend();

        /* Wind speed: pulses over 2s window → m/s
         * Davis 6410: 1 pulse = 0.27 m/s (1600 rev/hour = 1 mph)
         */
        g_wind_pulses = 0;
        vTaskDelay(pdMS_TO_TICKS(2000));
        uint16_t wind_speed = (uint16_t)(g_wind_pulses * 27); /* ×0.1 m/s */
        uint16_t wind_dir = read_wind_dir();

        /* Rain: tips since last report (reset each cycle) */
        uint16_t rain = g_rain_tips;
        g_rain_tips = 0;

        /* Battery voltage */
        /* Simplified */
        uint8_t bat_mv = 320;

        ESP_LOGI(TAG, "T:%.1f H:%.0f P:%.0f Wind:%.1fm/s Rain:%dmm Trend:%d",
                 temp, hum, pres, wind_speed/10.0, (int)(rain*0.2), trend);

        /* Build and send telemetry */
        ss_message_t msg;
        ss_build_weather_telem(&msg, g_mesh.node_id, g_msg_seq++,
                               bat_mv,
                               (int16_t)(temp * 10),
                               (uint16_t)(hum * 10),
                               (uint16_t)(pres * 10) - 8000,
                               wind_speed, wind_dir, rain, trend,
                               g_mesh.last_rssi);

        ss_mesh_wait_slot(&g_mesh);
        ss_mesh_send(&g_mesh, &msg);

        vTaskDelay(pdMS_TO_TICKS(WEATHER_SAMPLE_INTERVAL * 1000));
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "StormSync Weather Sentinel starting...");

    gpio_set_direction(WX_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(WX_GPIO_SX_RST, 1);
    gpio_set_direction(WX_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(WX_GPIO_LED, GPIO_MODE_OUTPUT);

    /* Wind speed interrupt */
    gpio_set_direction(WX_GPIO_WIND_SPD, GPIO_MODE_INPUT);
    gpio_set_intr_type(WX_GPIO_WIND_SPD, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(WX_GPIO_WIND_SPD, wind_isr, NULL);

    /* Rain gauge interrupt */
    gpio_set_direction(WX_GPIO_RAIN_TIP, GPIO_MODE_INPUT);
    gpio_set_intr_type(WX_GPIO_RAIN_TIP, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(WX_GPIO_RAIN_TIP, rain_isr, NULL);

    i2c_init();
    spi_init();

    ss_radio_config_t radio_cfg = {
        .frequency = SS_NET_FREQ_HZ,
        .bandwidth = SS_NET_BW_HZ,
        .spreading_factor = SS_NET_SF,
        .coding_rate = SS_NET_CR,
        .preamble_len = SS_NET_PREAMBLE,
        .tx_power_dbm = SS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    ss_mesh_init(&g_mesh, SS_NODE_WEATHER, &g_spi_iface, &radio_cfg);

    if (ss_mesh_join(&g_mesh) != 0) {
        ESP_LOGW(TAG, "Mesh join failed, will retry");
    }

    xTaskCreate(sensor_task, "sensor", 6144, NULL, 5, NULL);

    ESP_LOGI(TAG, "Weather Sentinel running. Free heap: %lu",
             (unsigned long)esp_get_free_heap_size());
}