/**
 * JointSync Compression Sleeve — Pump & Valve Driver
 *
 * DRV8833 motor driver + micro-pump + 2 solenoid valves.
 *
 * License: MIT
 */

#include "pump_driver.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "pump_driver";

#define PUMP_PWM_GPIO    6
#define VALVE1_GPIO      7
#define VALVE2_GPIO      8
#define MOTOR_ENABLE_GPIO 12
#define MOTOR_FAULT_GPIO 11

#define LEDC_TIMER       LEDC_TIMER_0
#define LEDC_CHANNEL     LEDC_CHANNEL_0
#define LEDC_FREQ_HZ     25000   /* 25 kHz PWM */
#define LEDC_RESOLUTION  LEDC_TIMER_10_BIT

void pump_driver_init(void)
{
    /* Configure PWM for pump motor */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_RESOLUTION,
        .freq_hz = LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t ch_cfg = {
        .channel = LEDC_CHANNEL,
        .duty = 0,
        .gpio_num = PUMP_PWM_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER,
        .hpoint = 0,
    };
    ledc_channel_config(&ch_cfg);

    /* Configure valve GPIOs */
    gpio_set_direction(VALVE1_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(VALVE2_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(MOTOR_ENABLE_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(MOTOR_FAULT_GPIO, GPIO_MODE_INPUT);

    /* Initial state: off */
    gpio_set_level(VALVE1_GPIO, 0);  /* Closed */
    gpio_set_level(VALVE2_GPIO, 0);  /* Closed */
    gpio_set_level(MOTOR_ENABLE_GPIO, 0);  /* Disabled */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);

    ESP_LOGI(TAG, "Pump driver initialized (DRV8833, 25 kHz PWM)");
}

void pump_driver_inflate(void)
{
    /* Open valve 1 (inlet), close valve 2 (outlet) */
    gpio_set_level(VALVE1_GPIO, 1);  /* Open inlet */
    gpio_set_level(VALVE2_GPIO, 0);  /* Close outlet */
    gpio_set_level(MOTOR_ENABLE_GPIO, 1);  /* Enable motor driver */
    /* PWM duty set separately by pump_driver_set_pwm() */
}

void pump_driver_deflate(void)
{
    /* Close valve 1 (inlet), open valve 2 (outlet) */
    gpio_set_level(VALVE1_GPIO, 0);  /* Close inlet */
    gpio_set_level(VALVE2_GPIO, 1);  /* Open outlet */
    gpio_set_level(MOTOR_ENABLE_GPIO, 0);  /* Disable pump */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
}

void pump_driver_hold(void)
{
    /* Close both valves — maintain pressure */
    gpio_set_level(VALVE1_GPIO, 0);  /* Close inlet */
    gpio_set_level(VALVE2_GPIO, 0);  /* Close outlet */
    gpio_set_level(MOTOR_ENABLE_GPIO, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
}

void pump_driver_off(void)
{
    /* Everything off */
    gpio_set_level(VALVE1_GPIO, 0);
    gpio_set_level(VALVE2_GPIO, 0);
    gpio_set_level(MOTOR_ENABLE_GPIO, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
}

void pump_driver_set_pwm(float duty)
{
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    uint32_t duty_val = (uint32_t)(duty * 1023.0f);  /* 10-bit resolution */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, duty_val);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
}

bool pump_driver_check_fault(void)
{
    return (gpio_get_level(MOTOR_FAULT_GPIO) == 0);  /* Active low fault */
}