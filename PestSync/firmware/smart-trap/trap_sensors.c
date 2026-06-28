/*
 * Smart Trap — Sensor Reading
 * Reed switch (trap fire), HX711 (catch weight), capacitive bait level, ADXL362 (tamper)
 * firmware/smart-trap/trap_sensors.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_rom_sys.h"
#include <string.h>

#include "psp_protocol.h"

static const char *TAG = "TRAP_SENSORS";

#define PIN_REED        0
#define PIN_HX711_DOUT  1
#define PIN_HX711_SCK   10
#define PIN_BAIT_ADC    9
#define PIN_LED         18

#define HX711_SCALE  2280.0f  /* counts per gram (calibrate per load cell) */

extern trap_data_t g_trap_data;
extern volatile bool trap_triggered;
extern volatile bool trap_tampered;

static int32_t tare_offset = 0;

/* ============ HX711 Load Cell ============ */

static void hx711_wake(void)
{
    gpio_set_level(PIN_HX711_SCK, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
}

static int32_t hx711_read(void)
{
    int timeout = 1000;
    while (gpio_get_level(PIN_HX711_DOUT) == 1) {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (--timeout <= 0) return 0;
    }

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

    if (value & 0x800000) value |= 0xFF000000;
    return value;
}

static uint16_t read_catch_weight(void)
{
    hx711_wake();
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Average 3 readings */
    int32_t sum = 0;
    for (int i = 0; i < 3; i++) {
        sum += hx711_read();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    int32_t avg = sum / 3;

    int32_t grams = (int32_t)((avg - tare_offset) / HX711_SCALE);
    if (grams < 0) grams = 0;
    if (grams > 500) grams = 500;
    return (uint16_t)grams;
}

/* ============ Bait Level (Capacitive) ============ */

static uint8_t read_bait_level(void)
{
    uint32_t raw = 0;
    for (int i = 0; i < 10; i++) {
        raw += adc1_get_raw(ADC1_CHANNEL_0);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    raw /= 10;

    /* Capacitive bait sensor:
     * Full bait: ADC ~2500 (high dielectric)
     * Empty: ADC ~4000 (air)
     * Map to 0-100% */
    int level = (int)((4000 - raw) * 100 / (4000 - 2500));
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    return (uint8_t)level;
}

/* ============ Catch Classification ============ */

static uint8_t classify_catch(uint16_t weight_g)
{
    if (weight_g < 5)  return CATCH_FALSE_TRIGGER;
    if (weight_g >= 15 && weight_g <= 30)  return CATCH_MOUSE;
    if (weight_g >= 150 && weight_g <= 300) return CATCH_RAT;
    if (weight_g >= 5 && weight_g < 15)  return CATCH_INSECT;  /* small insect on glue board */
    return CATCH_UNKNOWN;
}

/* ============ Main Sensor Task ============ */

void trap_sensors_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Trap sensor task started");

    /* Tare the load cell on startup */
    hx711_wake();
    vTaskDelay(pdMS_TO_TICKS(200));
    tare_offset = hx711_read();
    ESP_LOGI(TAG, "Load cell tared: offset=%ld", (long)tare_offset);

    /* Set LED green (armed) — on ESP32-C3 bicolor, GPIO high = green */
    gpio_set_level(PIN_LED, 1);

    while (1) {
        /* Check for trap trigger (set by ISR) */
        if (trap_triggered) {
            ESP_LOGI(TAG, "🎯 TRAP TRIGGERED!");

            /* Wait a moment for catch to settle on load cell */
            vTaskDelay(pdMS_TO_TICKS(500));

            /* Read catch weight */
            uint16_t weight = read_catch_weight();
            uint8_t catch_class = classify_catch(weight);

            /* Update trap data */
            g_trap_data.trap_status = TRAP_TRIGGERED;
            g_trap_data.catch_weight_g = weight;
            g_trap_data.catch_class = catch_class;
            g_trap_data.alerts |= ALERT_TRAP_TRIGGERED;

            /* Set LED red (triggered) */
            gpio_set_level(PIN_LED, 0);

            ESP_LOGI(TAG, "Catch: %dg → class: %s",
                     weight,
                     catch_class == CATCH_MOUSE ? "mouse" :
                     catch_class == CATCH_RAT ? "rat" :
                     catch_class == CATCH_INSECT ? "insect" :
                     catch_class == CATCH_FALSE_TRIGGER ? "false trigger" : "unknown");

            /* Clear trigger flag — will re-arm when user resets */
            trap_triggered = false;

            /* Don't re-arm automatically — wait for reset command from Hub */
            vTaskDelay(pdMS_TO_TICKS(60000)); /* report then sleep */
            continue;
        }

        /* Check for tamper */
        if (trap_tampered) {
            ESP_LOGW(TAG, "⚠️  Trap tampered!");
            g_trap_data.trap_status = TRAP_TAMPERED;
            g_trap_data.alerts |= ALERT_TRAP_TAMPERED;
            trap_tampered = false;
        }

        /* Periodic bait level check (every 6 hours) */
        static uint32_t last_bait_check = 0;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
        if (now - last_bait_check > 21600) { /* 6 hours */
            g_trap_data.bait_level = read_bait_level();
            if (g_trap_data.bait_level < 30)
                g_trap_data.alerts |= ALERT_BAIT_LOW;
            else
                g_trap_data.alerts &= ~ALERT_BAIT_LOW;
            last_bait_check = now;

            ESP_LOGI(TAG, "Bait level: %d%%", g_trap_data.bait_level);
        }

        /* Update uptime */
        g_trap_data.uptime_s = now;

        /* Short sleep — wake on ISR for trigger/tamper */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}