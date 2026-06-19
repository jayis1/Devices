/*
 * weight_scale.c — SoleGuard HX711 4-load-cell weight scale driver
 *
 * 4× 50kg load cells under the scan platform (wheatstone bridge -> HX711
 * 24-bit ADC). Tare on power-up. Returns weight in grams.
 *
 * SPDX-License-Identifier: MIT
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "sole_protocol.h"

#define HX711_SCK   GPIO_NUM_44
#define HX711_DOUT  GPIO_NUM_43

#define HX711_SCALE_FACTOR  2280.0f  /* calibration: grams per raw unit */
#define HX711_TARE_AVG      10

static int32_t tare_offset = 0;

static int hx711_read_raw(int32_t *raw) {
    /* Wait for DOUT low (data ready) */
    int timeout = 50000;
    while (gpio_get_level(HX711_DOUT) == 1) {
        if (--timeout <= 0) return -1;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    /* Read 24 bits */
    int32_t val = 0;
    for (int i = 0; i < 24; i++) {
        gpio_set_level(HX711_SCK, 1);
        ets_delay_us(1);
        val = (val << 1) | gpio_get_level(HX711_DOUT);
        gpio_set_level(HX711_SCK, 0);
        ets_delay_us(1);
    }
    /* 25th pulse for channel A gain 128 */
    gpio_set_level(HX711_SCK, 1);
    ets_delay_us(1);
    gpio_set_level(HX711_SCK, 0);
    /* Sign-extend 24-bit */
    if (val & 0x800000) val |= 0xFF000000;
    *raw = val;
    return 0;
}

static void hx711_init_gpio(void) {
    gpio_config_t dout = { .pin_bit_mask = (1ULL<<HX711_DOUT),
                           .mode = GPIO_MODE_INPUT, .pull_up_en = 0, .pull_down_en = 0 };
    gpio_config(&dout);
    gpio_config_t sck = { .pin_bit_mask = (1ULL<<HX711_SCK),
                          .mode = GPIO_MODE_OUTPUT, .pull_up_en = 0, .pull_down_en = 0 };
    gpio_config(&sck);
    gpio_set_level(HX711_SCK, 0);
}

void weight_scale_init(void) {
    hx711_init_gpio();
    /* Tare */
    int32_t sum = 0;
    for (int i = 0; i < HX711_TARE_AVG; i++) {
        int32_t raw;
        if (hx711_read_raw(&raw) == 0) sum += raw;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    tare_offset = sum / HX711_TARE_AVG;
}

int hx711_read_grams(int32_t *grams) {
    int32_t raw;
    if (hx711_read_raw(&raw) != 0) return -1;
    int32_t net = raw - tare_offset;
    *grams = (int32_t)((float)net / HX711_SCALE_FACTOR);
    return 0;
}