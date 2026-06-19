/*
 * load_cell.c — PawSync HX711 load cell interface
 *
 * Reads weight from 4× strain gauge load cells via the HX711 24-bit ADC.
 * Provides weight in grams with 0.1g resolution.
 *
 * The HX711 communicates via a simple proprietary 2-wire protocol:
 *   - SCK: clock (max 1MHz, 25-27 pulses per read)
 *   - DOUT: data (goes LOW when data is ready)
 *
 * Calibration: tare with empty bowl, then apply known weight to compute
 * the scale factor.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

/* ---- Pins (set by init) ---- */
static int sck_pin = -1;
static int dout_pin = -1;

/* ---- Calibration ---- */
static int32_t tare_offset = 0;
static float scale_factor = 1.0f;  /* raw → grams */
static bool initialized = false;

/* ---- HX711 raw read (24-bit signed) ---- */
static int32_t hx711_read_raw_internal(void)
{
    /* Wait for DOUT to go LOW (data ready) */
    int timeout = 0;
    while (gpio_get_level(dout_pin) == 1) {
        if (++timeout > 100000) return 0;  /* timeout */
        esp_rom_delay_us(10);
    }

    /* Read 24 bits */
    int32_t value = 0;
    for (int i = 0; i < 24; i++) {
        gpio_set_level(sck_pin, 1);
        esp_rom_delay_us(1);
        value = (value << 1) | gpio_get_level(dout_pin);
        gpio_set_level(sck_pin, 0);
        esp_rom_delay_us(1);
    }

    /* 25th pulse: channel A, gain 128 (sets next read config) */
    gpio_set_level(sck_pin, 1);
    esp_rom_delay_us(1);
    gpio_set_level(sck_pin, 0);
    esp_rom_delay_us(1);

    /* Convert 24-bit two's complement to signed 32-bit */
    if (value & 0x800000)
        value |= 0xFF000000;

    return value;
}

/* ---- Public API ---- */
void hx711_init(int sck, int dout)
{
    sck_pin = sck;
    dout_pin = dout;

    gpio_set_direction(sck_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(dout_pin, GPIO_MODE_INPUT);
    gpio_set_level(sck_pin, 0);

    /* Power cycle (HX711: power down + up resets) */
    gpio_set_level(sck_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(sck_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(400));  /* wait for settle */

    initialized = true;
}

int32_t hx711_read_raw(void)
{
    if (!initialized) return 0;

    /* Average 5 readings for noise reduction */
    int32_t sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += hx711_read_raw_internal();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return sum / 5;
}

float hx711_get_weight_g(void)
{
    int32_t raw = hx711_read_raw();
    float weight = (float)(raw - tare_offset) / scale_factor;
    if (weight < 0) weight = 0;
    return weight;
}

void hx711_tare(void)
{
    if (!initialized) return;
    /* Average 10 readings for stable tare */
    int32_t sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += hx711_read_raw_internal();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    tare_offset = sum / 10;
}

/* ---- Calibration helper ---- */
void hx711_calibrate(float known_weight_g)
{
    /* Place known weight on scale, call this function */
    int32_t raw = hx711_read_raw();
    int32_t diff = raw - tare_offset;
    if (diff != 0 && known_weight_g > 0)
        scale_factor = (float)diff / known_weight_g;
}