/*
 * Bin Node — Servo Vent Control
 * firmware/bin-node/servo.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "SERVO";

#define PIN_SERVO       18
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_FREQ_HZ    50  /* 50 Hz for servo */

/* MG90S servo: 500 µs = 0°, 2500 µs = 180° */
#define SERVO_MIN_US    500
#define SERVO_MAX_US    2500

extern uint8_t vent_position;

void servo_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num  = LEDC_TIMER,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .freq_hz = LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ch_conf = {
        .gpio_num = PIN_SERVO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ch_conf);
}

static void servo_set_position(uint8_t percent)
{
    if (percent > 100) percent = 100;
    /* Map 0-100% to 500-2500 µs pulse */
    uint32_t pulse_us = SERVO_MIN_US + (uint32_t)(percent) *
                        (SERVO_MAX_US - SERVO_MIN_US) / 100;
    /* Convert µs to duty (14-bit at 50 Hz: 20000 µs period) */
    uint32_t duty = (pulse_us * 16384) / 20000;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
}

void servo_task(void *pvParameters)
{
    servo_init();
    uint8_t last_pos = 255;

    while (1) {
        if (vent_position != last_pos) {
            ESP_LOGI(TAG, "Setting vent to %d%%", vent_position);
            servo_set_position(vent_position);
            last_pos = vent_position;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}