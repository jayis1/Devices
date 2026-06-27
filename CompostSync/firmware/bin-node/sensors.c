/*
 * Bin Node — Sensor Reading
 * DS18B20 (OneWire), Capacitive Moisture (ADC), SCD41 (I2C),
 * MQ-4 (ADC), HX711 (GPIO bit-bang)
 * firmware/bin-node/sensors.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/i2c.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "csp_protocol.h"
#include "sensor_types.h"

static const char *TAG = "SENSORS";

/* Pin definitions (must match main.c) */
#define PIN_DS18B20_1    32
#define PIN_DS18B20_2    33
#define PIN_DS18B20_3    34
#define PIN_MOISTURE_1   35
#define PIN_MOISTURE_2   36
#define PIN_MOISTURE_3   39
#define PIN_MQ4          25
#define PIN_HX711_DOUT   26
#define PIN_HX711_SCK    27

#define I2C_NUM     I2C_NUM_0
#define I2C_SDA     21
#define I2C_SCL     22
#define SCD41_ADDR  0x62

#define SAMPLE_INTERVAL_S  900  /* 15 min */

/* Moisture calibration values (air dry → 4095, water → ~1800 for capacitive v1.2) */
#define MOISTURE_DRY  4095
#define MOISTURE_WET  1800

/* HX711 calibration */
#define HX711_SCALE   2280.0f  /* counts per gram (calibrate per load cell) */

extern bin_node_data_t latest_data;
extern uint8_t vent_position;
extern int32_t tare_offset;

/* ============ DS18B20 OneWire ============ */

/* OneWire reset */
static int ow_reset(int pin)
{
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    vTaskDelay(pdMS_TO_TICKS(1)); /* 500 µs min */
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    vTaskDelay(pdMS_TO_TICKS(1));
    int presence = (gpio_get_level(pin) == 0) ? 1 : 0;
    vTaskDelay(pdMS_TO_TICKS(1));
    return presence;
}

/* OneWire write bit */
static void ow_write_bit(int pin, int bit)
{
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    esp_rom_delay_us(bit ? 6 : 60);
    if (bit) {
        gpio_set_direction(pin, GPIO_MODE_INPUT);
        esp_rom_delay_us(64);
    } else {
        gpio_set_direction(pin, GPIO_MODE_INPUT);
        esp_rom_delay_us(10);
    }
}

/* OneWire read bit */
static int ow_read_bit(int pin)
{
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    esp_rom_delay_us(3);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    esp_rom_delay_us(10);
    int bit = gpio_get_level(pin);
    esp_rom_delay_us(53);
    return bit;
}

/* OneWire write byte */
static void ow_write_byte(int pin, uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        ow_write_bit(pin, (byte >> i) & 1);
    }
}

/* OneWire read byte */
static uint8_t ow_read_byte(int pin)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        byte |= (ow_read_bit(pin) << i);
    }
    return byte;
}

/* Read DS18B20 temperature */
static float read_ds18b20(int pin)
{
    if (!ow_reset(pin)) return -999.0f;

    /* Skip ROM + Convert T */
    ow_write_byte(pin, 0xCC);
    ow_write_byte(pin, 0x44);

    /* Wait for conversion (750 ms for 12-bit) */
    vTaskDelay(pdMS_TO_TICKS(750));

    if (!ow_reset(pin)) return -999.0f;

    /* Skip ROM + Read Scratchpad */
    ow_write_byte(pin, 0xCC);
    ow_write_byte(pin, 0xBE);

    uint8_t lsb = ow_read_byte(pin);
    uint8_t msb = ow_read_byte(pin);

    int16_t raw = (msb << 8) | lsb;
    return raw / 16.0f;
}

/* ============ Capacitive Moisture ============ */

static uint8_t read_moisture(adc1_channel_t ch)
{
    uint32_t raw = 0;
    for (int i = 0; i < 10; i++) {
        raw += adc1_get_raw(ch);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    raw /= 10;

    /* Map: dry (MOISTURE_DRY=4095) → 0%, wet (MOISTURE_WET=1800) → 100% */
    int moisture = (int)((MOISTURE_DRY - raw) * 100 / (MOISTURE_DRY - MOISTURE_WET));
    if (moisture < 0) moisture = 0;
    if (moisture > 100) moisture = 100;
    return (uint8_t)moisture;
}

/* ============ SCD41 CO2 (I2C) ============ */

static void scd41_init(void)
{
    /* Init sequence: SCD41 needs periodic measurement start */
    uint8_t cmd[2] = { 0x21, 0xB1 }; /* Start periodic measurement */
    i2c_master_write_to_device(I2C_NUM, SCD41_ADDR, cmd, 2, pdMS_TO_TICKS(100));
}

static int scd41_read(uint16_t *co2, float *temp, float *humidity)
{
    /* Read measurement sequence */
    uint8_t cmd[2] = { 0xEC, 0x05 }; /* Read measurement */
    uint8_t data[9];

    esp_err_t err = i2c_master_write_read_device(I2C_NUM, SCD41_ADDR,
                                                  cmd, 2, data, 9, pdMS_TO_TICKS(100));
    if (err != ESP_OK) return -1;

    *co2 = (data[0] << 8) | data[1];
    int16_t temp_raw = (data[3] << 8) | data[4];
    int16_t hum_raw = (data[6] << 8) | data[7];
    *temp = -45.0f + 175.0f * temp_raw / 65535.0f;
    *humidity = 100.0f * hum_raw / 65535.0f;
    return 0;
}

/* ============ MQ-4 Methane ============ */

static uint16_t read_methane(void)
{
    /* MQ-4 needs a warm-up period. We read the analog value and
     * convert to ppm using a simplified log relationship.
     * Rs/R0 ratio → ppm using the calibration curve from datasheet. */
    uint32_t raw = 0;
    for (int i = 0; i < 5; i++) {
        raw += adc1_get_raw(ADC1_CHANNEL_8); /* GPIO25, ADC2_CH8 */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    raw /= 5;

    /* Convert ADC to voltage */
    float vout = raw * 3.3f / 4095.0f;
    float rl = 10.0f;  /* load resistor kOhm */
    float vcc = 3.3f;
    float rs = rl * (vcc - vout) / vout; /* sensor resistance */

    /* R0 calibrated in clean air (calibrate once) */
    float r0 = 10.0f; /* calibrated value */
    float ratio = rs / r0;

    /* Simplified CH4 ppm from ratio (from datasheet log-log curve) */
    float ppm = 1000.0f * powf(ratio / 4.4f, -2.5f);
    if (ppm < 0) ppm = 0;
    if (ppm > 10000) ppm = 10000;
    return (uint16_t)ppm;
}

/* ============ HX711 Load Cell ============ */

static void hx711_init(void)
{
    /* HX711 power up: SCK low for >60 µs to wake */
    gpio_set_level(PIN_HX711_SCK, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
}

static int32_t hx711_read(void)
{
    /* Wait for DOUT to go low (data ready) */
    int timeout = 1000;
    while (gpio_get_level(PIN_HX711_DOUT) == 1) {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (--timeout <= 0) return 0;
    }

    /* Read 24 bits */
    int32_t value = 0;
    for (int i = 0; i < 24; i++) {
        gpio_set_level(PIN_HX711_SCK, 1);
        esp_rom_delay_us(1);
        value = (value << 1) | gpio_get_level(PIN_HX711_DOUT);
        gpio_set_level(PIN_HX711_SCK, 0);
        esp_rom_delay_us(1);
    }

    /* 25th pulse for channel A gain 128 */
    gpio_set_level(PIN_HX711_SCK, 1);
    esp_rom_delay_us(1);
    gpio_set_level(PIN_HX711_SCK, 0);

    /* Sign-extend 24-bit to 32-bit */
    if (value & 0x800000) value |= 0xFF000000;

    return value;
}

static uint16_t read_weight(void)
{
    int32_t raw = hx711_read();
    if (raw == 0) return 0;
    int32_t grams = (int32_t)((raw - tare_offset) / HX711_SCALE);
    if (grams < 0) grams = 0;
    if (grams > 50000) grams = 50000; /* cap at 50 kg */
    return (uint16_t)grams;
}

/* ============ Battery voltage ============ */

static uint8_t read_battery(void)
{
    /* Read battery voltage via ADC (assuming divider or internal Vbat)
     * ESP32 internal: ADC2_CH7 or external divider.
     * Simplified: return simulated value based on uptime */
    uint32_t uptime = latest_data.uptime_s;
    /* Linear model: 100% → 50% over 72 hours */
    uint8_t pct = (uint8_t)(100 - (uptime % 259200) * 50 / 259200);
    return pct;
}

/* ============ Main sensors task ============ */

void sensors_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sensor task started");

    /* Init I2C for SCD41 */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM, &conf);
    i2c_driver_install(I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);

    /* Init sensors */
    scd41_init();
    hx711_init();

    /* Tare the load cell on first reading */
    if (!tare_offset) {
        tare_offset = hx711_read();
        ESP_LOGI(TAG, "Load cell tared: offset=%ld", (long)tare_offset);
    }

    while (1) {
        /* Read DS18B20 temperatures at 3 depths */
        latest_data.temp_c[0] = (int16_t)(read_ds18b20(PIN_DS18B20_1) * 10);
        latest_data.temp_c[1] = (int16_t)(read_ds18b20(PIN_DS18B20_2) * 10);
        latest_data.temp_c[2] = (int16_t)(read_ds18b20(PIN_DS18B20_3) * 10);

        /* Read moisture at 3 depths */
        latest_data.moisture_pct[0] = read_moisture(ADC1_CHANNEL_7); /* GPIO35 */
        latest_data.moisture_pct[1] = read_moisture(ADC1_CHANNEL_0); /* GPIO36 */
        latest_data.moisture_pct[2] = read_moisture(ADC1_CHANNEL_3); /* GPIO39 */

        /* Read CO2 from SCD41 */
        uint16_t co2;
        float co2_temp, co2_hum;
        if (scd41_read(&co2, &co2_temp, &co2_hum) == 0) {
            latest_data.co2_ppm = co2;
        } else {
            latest_data.co2_ppm = 0;
        }

        /* Read methane from MQ-4 */
        latest_data.methane_ppm = read_methane();

        /* Read weight from HX711 */
        latest_data.mass_grams = read_weight();

        /* Update metadata */
        latest_data.uptime_s = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
        latest_data.battery_pct = read_battery();
        latest_data.vent_position = vent_position;

        /* Check for critical conditions */
        latest_data.alerts = 0;
        if (latest_data.methane_ppm > 1000)
            latest_data.alerts |= ALERT_METHANE_HIGH;
        if (latest_data.temp_c[1] > 700) /* 70°C */
            latest_data.alerts |= ALERT_OVERHEAT;
        if (latest_data.battery_pct < 20)
            latest_data.alerts |= ALERT_LOW_BATTERY;
        if (latest_data.moisture_pct[1] > 70)
            latest_data.alerts |= ALERT_MOISTURE_HIGH;
        if (latest_data.moisture_pct[1] > 0 && latest_data.moisture_pct[1] < 30)
            latest_data.alerts |= ALERT_MOISTURE_LOW;

        ESP_LOGI(TAG, "T1=%.1f T2=%.1f T3=%.1f M1=%d%% M2=%d%% M3=%d%% "
                 "CO2=%d CH4=%d W=%dg B=%d%%",
                 latest_data.temp_c[0]/10.0f, latest_data.temp_c[1]/10.0f,
                 latest_data.temp_c[2]/10.0f,
                 latest_data.moisture_pct[0], latest_data.moisture_pct[1],
                 latest_data.moisture_pct[2],
                 latest_data.co2_ppm, latest_data.methane_ppm,
                 latest_data.mass_grams, latest_data.battery_pct);

        /* If methane high, open vent automatically */
        if (latest_data.methane_ppm > 1000 && vent_position < 50) {
            ESP_LOGW(TAG, "⚠️  Methane high — opening vent automatically");
            vent_position = 80;
        }
        /* If CO2 very high, open vent */
        if (latest_data.co2_ppm > 5000 && vent_position < 50) {
            ESP_LOGW(TAG, "⚠️  CO2 high (%d ppm) — opening vent", latest_data.co2_ppm);
            vent_position = 60;
        }

        /* Wait for next sample interval */
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_S * 1000));
    }
}