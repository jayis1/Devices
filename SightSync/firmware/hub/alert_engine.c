/**
 * SightSync Vision Hub — Alert Engine Implementation
 *
 * License: MIT
 */

#include "alert_engine.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include <math.h>

static const char *TAG = "alert";

/* Alert thresholds */
#define ALERT_FATIGUE_LOW       30
#define ALERT_FATIGUE_MODERATE  50
#define ALERT_FATIGUE_HIGH       70
#define ALERT_FATIGUE_CRITICAL   85

#define ALERT_DISTANCE_TOO_CLOSE 300   /* mm */
#define ALERT_NEAR_WORK_LIMIT    45    /* minutes */

#define ALERT_BLINK_LOW          8     /* bpm */
#define ALERT_DRY_EYE_THRESHOLD  60    /* 0-100 */

/* I2C address of DRV2605L haptic driver */
#define DRV2605_ADDR 0x5A

static void haptic_pulse(uint8_t pattern)
{
    /* DRV2605L: write pattern to register 0x04, then GO at 0x0C */
    uint8_t cmd1[] = {0x04, pattern};
    /* i2c_master_write_to_device(I2C_NUM_0, DRV2605_ADDR, cmd1, 2, 100); */
    uint8_t cmd2[] = {0x0C, 0x01};
    /* i2c_master_write_to_device(I2C_NUM_0, DRV2605_ADDR, cmd2, 2, 100); */
    (void)cmd1; (void)cmd2;
}

static void audio_beep(uint16_t freq_hz, uint16_t duration_ms)
{
    /* MAX98357A I²S — generate a sine wave tone for duration_ms */
    (void)freq_hz;
    (void)duration_ms;
}

static void led_ring_flash(uint8_t color_r, uint8_t color_g, uint8_t color_b, uint8_t count)
{
    /* SK6812 RGB ring flash */
    (void)color_r; (void)color_g; (void)color_b; (void)count;
}

void alert_engine_init(void)
{
    /* Initialize DRV2605L haptic driver, MAX98357A I²S amp, SK6812 LED ring */
    ESP_LOGI(TAG, "Alert engine initialized");
}

void alert_engine_evaluate(hub_state_t *s)
{
    /* Fatigue alert */
    if (s->fatigue_score >= ALERT_FATIGUE_CRITICAL) {
        ESP_LOGW(TAG, "CRITICAL fatigue: %d — haptic + audio + red LED", s->fatigue_score);
        haptic_pulse(0x0B);  /* strong buzz */
        audio_beep(880, 300);
        led_ring_flash(255, 0, 0, 3);
    } else if (s->fatigue_score >= ALERT_FATIGUE_HIGH) {
        ESP_LOGI(TAG, "HIGH fatigue: %d — haptic + yellow LED", s->fatigue_score);
        haptic_pulse(0x09);
        led_ring_flash(255, 200, 0, 2);
    } else if (s->fatigue_score >= ALERT_FATIGUE_MODERATE) {
        led_ring_flash(255, 200, 0, 1);
    }

    /* Distance alert */
    if (s->viewing_distance_mm > 0 && s->viewing_distance_mm < ALERT_DISTANCE_TOO_CLOSE) {
        ESP_LOGW(TAG, "Too close: %d mm — haptic pulse", s->viewing_distance_mm);
        haptic_pulse(0x05);
        led_ring_flash(0, 100, 255, 2);
    }

    /* Near-work limit */
    if (s->near_work_minutes_today > ALERT_NEAR_WORK_LIMIT) {
        ESP_LOGW(TAG, "Near-work limit exceeded: %lu min", (unsigned long)s->near_work_minutes_today);
        audio_beep(660, 200);
    }

    /* Dry-eye alert */
    if (s->dry_eye_risk >= ALERT_DRY_EYE_THRESHOLD) {
        ESP_LOGW(TAG, "Dry-eye risk: %d — recommend blink exercise", s->dry_eye_risk);
        haptic_pulse(0x07);
        led_ring_flash(100, 200, 255, 2);
    }

    /* Forward head posture */
    if (s->forward_head_flag) {
        ESP_LOGI(TAG, "Forward head posture detected — gentle haptic");
        haptic_pulse(0x03);
    }
}

void alert_engine_break_reminder(void)
{
    ESP_LOGI(TAG, "20-20-20 break reminder: look 20ft away for 20s");
    haptic_pulse(0x0A);  /* double buzz */
    audio_beep(440, 500);
    led_ring_flash(0, 255, 100, 3);
}