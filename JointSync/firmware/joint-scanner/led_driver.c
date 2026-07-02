/**
 * JointSync Joint Scanner — Multispectral LED Driver
 *
 * 3× AL8805 LED drivers for White, UV (365nm), and NIR (850nm) illumination.
 *
 * License: MIT
 */

#include "led_driver.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "led_driver";

#define LEDC_TIMER_W   LEDC_TIMER_1
#define LEDC_TIMER_UV  LEDC_TIMER_2
#define LEDC_TIMER_NIR LEDC_TIMER_3

#define LEDC_CH_W   LEDC_CHANNEL_1
#define LEDC_CH_UV  LEDC_CHANNEL_2
#define LEDC_CH_NIR LEDC_CHANNEL_3

#define LEDC_FREQ_HZ   1000   /* 1 kHz */
#define LEDC_RES       LEDC_TIMER_10_BIT

#define LED_W_GPIO   19
#define LED_UV_GPIO  20
#define LED_NIR_GPIO 21

void led_driver_init(void)
{
    /* Configure LEDC timers */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_RES,
        .freq_hz = LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    timer_cfg.timer_num = LEDC_TIMER_W;
    ledc_timer_config(&timer_cfg);
    timer_cfg.timer_num = LEDC_TIMER_UV;
    ledc_timer_config(&timer_cfg);
    timer_cfg.timer_num = LEDC_TIMER_NIR;
    ledc_timer_config(&timer_cfg);

    /* Configure LEDC channels */
    ledc_channel_config_t ch_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty = 0,
        .hpoint = 0,
        .timer_sel = LEDC_TIMER_W,
    };

    /* White LED */
    ch_cfg.channel = LEDC_CH_W;
    ch_cfg.gpio_num = LED_W_GPIO;
    ch_cfg.timer_sel = LEDC_TIMER_W;
    ledc_channel_config(&ch_cfg);

    /* UV LED */
    ch_cfg.channel = LEDC_CH_UV;
    ch_cfg.gpio_num = LED_UV_GPIO;
    ch_cfg.timer_sel = LEDC_TIMER_UV;
    ledc_channel_config(&ch_cfg);

    /* NIR LED */
    ch_cfg.channel = LEDC_CH_NIR;
    ch_cfg.gpio_num = LED_NIR_GPIO;
    ch_cfg.timer_sel = LEDC_TIMER_NIR;
    ledc_channel_config(&ch_cfg);

    ESP_LOGI(TAG, "LED drivers initialized (White/UV/NIR, 1 kHz PWM)");
}

void led_driver_set(led_type_t type, uint8_t brightness)
{
    uint32_t duty = (uint32_t)brightness * 1023 / 255;

    switch (type) {
    case LED_WHITE:
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_W, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_W);
        break;
    case LED_UV:
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_UV, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_UV);
        break;
    case LED_NIR:
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_NIR, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_NIR);
        break;
    }
}

void led_driver_off(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_W, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_W);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_UV, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_UV);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_NIR, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_NIR);
}

void led_driver_flash(led_type_t type, uint16_t duration_ms)
{
    led_driver_set(type, 255);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    led_driver_off();
}