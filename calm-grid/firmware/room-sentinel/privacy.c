/*
 * privacy.c — CalmGrid Room Sentinel privacy system
 *
 * Implements physical privacy controls for voice processing:
 * 1. Hardware mic mute switch (GPIO39)
 * 2. Active LED indicator when microphone is processing
 * 3. On-device-only enforcement (no audio stream to cloud)
 * 4. Prosody features only — no speech content ever leaves the device
 *
 * When the mic is muted:
 *   - Audio capture is disabled
 *   - Prosody classification reports "calm" (no data)
 *   - A "privacy mode" flag is sent to the hub
 *
 * Privacy guarantee: The room sentinel NEVER transmits audio. Only
 * prosody feature vectors (9 floats) and a stress classification
 * (0-3) are sent. No speech-to-text, no audio recording, no cloud
 * audio streaming. This is enforced at the firmware level.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "calm_protocol.h"

static const char *TAG = "privacy";

#define MIC_MUTE_PIN    39
#define ACTIVE_LED_PIN  40

static bool mic_muted = false;
static bool privacy_initialized = false;

/* ---- Initialize privacy hardware ---- */
void privacy_init(void)
{
    /* Mic mute switch: input with pull-up (active low) */
    gpio_config_t input_conf = {
        .pin_bit_mask = (1ULL << MIC_MUTE_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&input_conf);

    /* Active LED: output */
    gpio_config_t output_conf = {
        .pin_bit_mask = (1ULL << ACTIVE_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&output_conf);

    /* Read initial state */
    mic_muted = (gpio_get_level(MIC_MUTE_PIN) == 0);
    gpio_set_level(ACTIVE_LED_PIN, 0);  /* LED off initially */

    privacy_initialized = true;
    ESP_LOGI(TAG, "Privacy initialized. Mic muted: %s", mic_muted ? "YES" : "NO");
}

/* ---- Check if mic is muted ---- */
bool privacy_is_mic_muted(void)
{
    if (!privacy_initialized) return true;
    return mic_muted;
}

/* ---- Update mute state (call periodically or on interrupt) ---- */
void privacy_update_mute_state(void)
{
    if (!privacy_initialized) return;
    bool new_state = (gpio_get_level(MIC_MUTE_PIN) == 0);
    if (new_state != mic_muted) {
        mic_muted = new_state;
        ESP_LOGI(TAG, "Mic mute switch: %s", mic_muted ? "MUTED" : "ACTIVE");
        if (mic_muted) {
            gpio_set_level(ACTIVE_LED_PIN, 0);  /* LED off when muted */
        }
    }
}

/* ---- Set active LED (indicates audio processing) ---- */
void privacy_set_active_led(bool on)
{
    if (!privacy_initialized) return;
    if (mic_muted) {
        gpio_set_level(ACTIVE_LED_PIN, 0);  /* never show active when muted */
        return;
    }
    gpio_set_level(ACTIVE_LED_PIN, on ? 1 : 0);
}

/* ---- Enforce on-device-only policy ---- */
/*
 * This function is called before any data transmission to verify
 * that only prosody features (not audio) are being sent.
 * In production, this is a compile-time guarantee: no audio buffer
 * is ever referenced in any network/UART transmission function.
 */
bool privacy_verify_transmission(uint8_t msg_type)
{
    /* Only these message types are allowed to be transmitted:
     * - PROSODY (features + classification only)
     * - ENVIRONMENT (ambient sensor data)
     * - HEARTBEAT (status)
     * - ALERT (flags only)
     *
     * Audio data (raw samples) is NEVER transmitted.
     */
    switch (msg_type) {
    case CALM_MSG_PROSODY:
    case CALM_MSG_ENVIRONMENT:
    case CALM_MSG_HEARTBEAT:
    case CALM_MSG_ALERT:
        return true;
    default:
        ESP_LOGW(TAG, "Blocked transmission of msg type 0x%02X (privacy policy)", msg_type);
        return false;
    }
}