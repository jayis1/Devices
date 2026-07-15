/*
 * LawnSync — Weather Station Firmware
 * ESP32-S3, FreeRTOS
 *
 * Measures: temperature, humidity, pressure, wind speed/direction,
 * rainfall, solar irradiance, UV index.
 * Reports every 5 minutes via Sub-GHz mesh.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/pcnt.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "LawnSync-Weather";

/* === Global state === */
static ls_mesh_ctx_t g_mesh;
static volatile uint16_t g_wind_pulse_count = 0;
static volatile uint16_t g_rain_tip_count = 0;

/* === SPI Interface === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = WX_GPIO_SX_MOSI,
        .miso_io_num = WX_GPIO_SX_MISO,
        .sclk_io_num = WX_GPIO_SX_SCK,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = WX_GPIO_SX_NSS,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi_dev);
}

static void spi_cs_select(void) { }
static void spi_cs_release(void) { }
static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_transaction_t t = { .tx_buffer = &byte, .rx_buffer = &rx, .length = 8 };
    spi_device_polling_transmit(g_spi_dev, &t);
    return rx;
}
static void spi_reset(uint8_t a) { gpio_set_level(WX_GPIO_SX_RST, a ? 0 : 1); }
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(WX_GPIO_SX_DIO1); }
static void spi_dio1_irq_enable(int e) { (void)e; }

static const ls_spi_interface_t g_spi_iface = {
    .init = spi_init, .cs_select = spi_cs_select, .cs_release = spi_cs_release,
    .transfer = spi_transfer, .reset = spi_reset, .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read, .dio1_irq_enable = spi_dio1_irq_enable,
};

/* === I²C for BME280 + VEML6075 === */
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

/* === Sensor Reading Functions === */

/* BME280: temp, humidity, pressure (I²C 0x76) */
static void read_bme280(float *temp, float *hum, float *pres)
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

    /* Parse raw data (simplified — production uses compensation tables) */
    int32_t raw_pres = (buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4);
    int32_t raw_temp = (buf[3] << 12) | (buf[4] << 4) | (buf[5] >> 4);
    int32_t raw_hum  = (buf[6] << 8) | buf[7];

    /* Apply compensation (simplified) */
    *temp = raw_temp / 100.0 - 40.0;
    *pres = raw_pres / 256.0;
    *hum  = raw_hum / 1024.0 * 100.0;
}

/* VEML6075: UV index (I²C 0x10) */
static float read_uv_index(void)
{
    uint8_t buf[4];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x10 << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x0A, true); /* UVA data */
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x10 << 1 | I2C_MASTER_READ, true);
    i2c_master_read(cmd, buf, 4, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    uint16_t uva = (buf[0] << 8) | buf[1];
    uint16_t uvb = (buf[2] << 8) | buf[3];
    /* Simplified UV index calculation */
    return (uva + uvb) * 0.025;
}

/* Wind speed: Davis 6410 reed switch, ~1 pulse per 2.25 km/h (0.625 m/s) */
static float read_wind_speed(void)
{
    /* Use PCNT (pulse counter) peripheral */
    int16_t count = 0;
    pcnt_count_t unit = PCNT_UNIT_0;
    pcnt_get_counter_value(unit, &count);

    /* Reset counter for next interval */
    pcnt_counter_clear(unit);

    /* Convert: speed_mps = count * 2.25 / 3.6 / interval_s
     * For 2-second window: speed_mps = count * 0.3125
     */
    return count * 0.3125;
}

/* Wind direction: Davis 6410 vane potentiometer, 0-360° via ADC */
static uint16_t read_wind_dir(void)
{
    /* ADC on GPIO9: 0-3.3V → 0-360° */
    /* In production: use adc_oneshot_read */
    uint16_t adc_raw = 2048; /* placeholder */
    return (uint16_t)((float)adc_raw / 4095.0 * 360.0);
}

/* Rain: tipping bucket, 0.2 mm per tip */
static uint16_t read_rain(void)
{
    uint16_t tips = g_rain_tip_count;
    g_rain_tip_count = 0;
    return tips; /* Caller multiplies by 0.2 mm */
}

/* Solar irradiance: Si cell via ADC */
static uint16_t read_solar_irr(void)
{
    /* ADC on GPIO11: 0-3.3V → 0-2000 W/m² */
    uint16_t adc_raw = 1500; /* placeholder */
    return (uint16_t)((float)adc_raw / 4095.0 * 2000.0);
}

/* Battery voltage */
static uint8_t read_battery_mv(void)
{
    /* ADC on GPIO19 through divider */
    uint16_t adc_raw = 2200; /* placeholder */
    uint32_t mv = (uint32_t)adc_raw * 6600 / 4095;
    return (uint8_t)(mv / 10); /* ×0.01V */
}

/* === PCNT Setup for Wind Speed === */
static void pcnt_init(void)
{
    pcnt_config_t cfg = {
        .pulse_gpio_num = WX_GPIO_WIND_SPD,
        .ctrl_gpio_num = PCNT_PIN_NOT_USED,
        .channel = PCNT_CHANNEL_0,
        .unit = PCNT_UNIT_0,
        .pos_mode = PCNT_COUNT_INC,
        .neg_mode = PCNT_COUNT_DIS,
        .lctrl_mode = PCNT_MODE_KEEP,
        .hctrl_mode = PCNT_MODE_KEEP,
        .counter_h_lim = 32767,
        .counter_l_lim = -32768,
    };
    pcnt_unit_config(&cfg);
    pcnt_counter_clear(PCNT_UNIT_0);
    pcnt_counter_resume(PCNT_UNIT_0);
}

/* === Rain ISR === */
static void IRAM_ATTR rain_isr(void *arg)
{
    g_rain_tip_count++;
}

/* === Main Sensor Task === */
static void sensor_task(void *arg)
{
    /* Initialize mesh */
    ls_radio_config_t radio_cfg = {
        .frequency = LS_NET_FREQ_HZ,
        .bandwidth = LS_NET_BW_HZ,
        .spreading_factor = LS_NET_SF,
        .coding_rate = LS_NET_CR,
        .preamble_len = LS_NET_PREAMBLE,
        .tx_power_dbm = LS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    ls_mesh_init(&g_mesh, LS_NODE_WEATHER, &g_spi_iface, &radio_cfg);
    ls_mesh_join(&g_mesh);

    ESP_LOGI(TAG, "Weather node joined: id=%d slot=%d",
             g_mesh.node_id, g_mesh.tdma_slot);

    while (1) {
        /* Read all sensors */
        float temp, hum, pres;
        read_bme280(&temp, &hum, &pres);
        float uv = read_uv_index();
        float wind_speed = read_wind_speed();
        uint16_t wind_dir = read_wind_dir();
        uint16_t rain_tips = read_rain();
        uint16_t solar_irr = read_solar_irr();
        uint8_t bat_mv = read_battery_mv();

        /* Build telemetry message */
        ls_message_t msg;
        ls_build_weather_telem(&msg, g_mesh.node_id, g_mesh.msg_seq++,
                                bat_mv,
                                (int16_t)(temp * 10),
                                (uint16_t)(hum * 10),
                                (uint16_t)((pres - 800) * 10), /* offset 800 hPa */
                                (uint16_t)(wind_speed * 10),
                                wind_dir,
                                rain_tips,
                                solar_irr,
                                (uint8_t)(uv * 10),
                                g_mesh.last_rssi);

        /* Wait for TDMA slot and send */
        ls_mesh_wait_slot(&g_mesh);
        ls_mesh_send(&g_mesh, &msg);

        ESP_LOGI(TAG, "T=%ld°C H=%ld%% P=%ldhPa W=%ldm/s R=%dtips UV=%.1f",
                 (long)temp, (long)hum, (long)pres,
                 (long)wind_speed, rain_tips, uv);

        /* Check for commands (between readings) */
        ls_message_t cmd;
        if (ls_mesh_recv(&g_mesh, &cmd, 1000) == 0) {
            if (cmd.header.type == LS_MSG_COMMAND && cmd.payload[0] == LS_CMD_REBOOT) {
                ESP_LOGI(TAG, "Reboot command received");
                esp_restart();
            }
        }

        /* Deep sleep until next reading (5 minutes) */
        /* In production: esp_deep_sleep(300 * 1000000) if battery-powered
         * For now, use vTaskDelay */
        vTaskDelay(pdMS_TO_TICKS(WEATHER_SAMPLE_INTERVAL * 1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "LawnSync Weather Station starting...");

    /* Initialize GPIOs */
    gpio_set_direction(WX_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(WX_GPIO_SX_RST, 1);
    gpio_set_direction(WX_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(WX_GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(WX_GPIO_RAIN_TIP, GPIO_MODE_INPUT);

    /* Initialize I²C */
    i2c_init();

    /* Initialize SPI */
    spi_init();

    /* Initialize PCNT for wind speed */
    pcnt_init();

    /* Install rain ISR */
    gpio_install_isr_service(0);
    gpio_isr_handler_add(WX_GPIO_RAIN_TIP, rain_isr, NULL);

    /* Start sensor task */
    xTaskCreate(sensor_task, "sensor", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Weather station running");
}