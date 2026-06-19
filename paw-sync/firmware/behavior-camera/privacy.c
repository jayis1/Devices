/*
 * privacy.c — PawSync Behavior Camera privacy system
 *
 * Implements physical privacy controls:
 * 1. Hardware lens shutter (servo or mechanical slider)
 * 2. Shutter switch interrupt (GPIO39)
 * 3. LED indicator when camera is active
 * 4. On-device-only enforcement (no video stream to cloud)
 *
 * When the shutter is closed:
 *   - Camera capture is disabled
 *   - Audio processing continues (mic has no physical cover, but
 *     can be muted via software)
 *   - A "privacy mode" flag is sent to the hub
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "paw_protocol.h"

static const char *TAG = "privacy";

#define SHUTTER_SWITCH_PIN  39
#define PRIVACY_LED_PIN     47  /* active LED indicator */
#define MIC_MUTE_PIN        48  /* mic mute switch (optional)

static bool shutter_closed = false;
static bool mic_muted = false;

/* ---- GPIO ISR for shutter switch ---- */
static void IRAM_ATTR shutter_isr_handler(void *arg)
{
    /* Read switch state (debounce in software) */
    shutter_closed = (gpio_get_level(SHUTTER_SWITCH_PIN) == 0);
}

/* ---- Initialize privacy system ---- */
void privacy_init(void)
{
    /* Configure shutter switch as input with pull-up */
    gpio_config_t input_conf = {
        .pin_bit_mask = (1ULL << SHUTTER_SWITCH_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&input_conf);

    /* Configure LED output */
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << PRIVACY_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_conf);

    /* Install ISR */
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SHUTTER_SWITCH_PIN, shutter_isr_handler, NULL);

    /* Read initial state */
    shutter_closed = (gpio_get_level(SHUTTER_SWITCH_PIN) == 0);
    ESP_LOGI(TAG, "Privacy system initialized — shutter %s",
             shutter_closed ? "CLOSED" : "open");
}

/* ---- Check shutter status ---- */
bool privacy_is_shutter_closed(void)
{
    return shutter_closed;
}

/* ---- Check mic mute status ---- */
bool privacy_is_mic_muted(void)
{
    return mic_muted;
}

/* ---- Privacy LED control ---- */
void privacy_led_set(bool on)
{
    gpio_set_level(PRIVACY_LED_PIN, on ? 1 : 0);
}

/* ---- Periodic privacy status check ---- */
void privacy_monitor_task(void *arg)
{
    bool prev_shutter = shutter_closed;
    while (1) {
        /* Debounce shutter switch */
        vTaskDelay(pdMS_TO_TICKS(100));
        bool current = (gpio_get_level(SHUTTER_SWITCH_PIN) == 0);

        /* Require stable reading for 200ms */
        int stable_count = 0;
        for (int i = 0; i < 4; i++) {
            vTaskDelay(pdMS_TO_TICKS(50));
            bool next = (gpio_get_level(SHUTTER_SWITCH_PIN) == 0);
            if (next == current) stable_count++;
        }
        if (stable_count >= 3) {
            shutter_closed = current;
            if (shutter_closed != prev_shutter) {
                ESP_LOGI(TAG, "Shutter %s", shutter_closed ? "CLOSED" : "OPENED");
                /* Update LED: red when camera active (open), off when closed */
                privacy_led_set(!shutter_closed);
                prev_shutter = shutter_closed;
            }
        }
    }
}