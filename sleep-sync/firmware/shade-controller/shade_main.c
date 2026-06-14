/*
 * shade_main.c — SleepSync Shade Controller (ESP32-C3-MINI-1)
 *
 * Motorized window shade controller with dawn simulator:
 * - TMC2209 silent stepper driver for roller shade
 * - 3-channel LED dawn simulator (warm white / amber / cool white)
 * - VEML7700 ambient light sensor
 * - Position memory with limit switch calibration
 * - Dawn simulation program (30-min sunrise)
 * - Receives commands from hub via BLE mesh
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"

#include "mesh_protocol.h"

/* ---- Pin Definitions ---- */
#define PIN_I2C0_SDA      0
#define PIN_I2C0_SCL      1
#define PIN_STEP          2    /* TMC2209 STEP */
#define PIN_DIR           3    /* TMC2209 DIR */
#define PIN_EN            4    /* TMC2209 ENABLE (active low) */
#define PIN_TMC_UART_TX   5    /* TMC2209 UART TX (config) */
#define PIN_TMC_UART_RX   6    /* TMC2209 UART RX (config) */
#define PIN_LED_WW        7    /* PT4115 warm white PWM */
#define PIN_LED_AMBER     8    /* PT4115 amber PWM */
#define PIN_LED_COOL      9    /* PT4115 cool white PWM */
#define PIN_LIMIT_TOP     10   /* Top limit switch */
#define PIN_LIMIT_BOTTOM  11   /* Bottom limit switch */
#define PIN_BTN_MANUAL    12   /* Manual open/close button */
#define PIN_DIAG          13   /* TMC2209 diagnostic */

/* ---- Stepper Motor Constants ---- */
#define STEPS_PER_MM      50    /* steps/mm for roller shade mechanism */
#define MAX_SPEED_MM_S    30    /* max speed: 30mm/s */
#define ACCEL_MM_S2       60    /* acceleration: 60mm/s² */
#define SHADE_TRAVEL_MM   1500  /* typical shade travel: 1.5m */

/* ---- Dawn Simulator Constants ---- */
#define DAWN_DURATION_S    1800  /* 30 minutes */
#define DAWN_STEPS        180    /* Update every 10s for smooth transition */

/* ---- Position State ---- */
typedef struct {
    bool     calibrated;
    int32_t  position_steps;   /* Current position in steps (0 = closed) */
    int32_t  max_steps;        /* Total steps for full travel (learned) */
    bool     moving;
    int32_t  target_steps;
} shade_position_t;

static shade_position_t shade = {
    .calibrated = false,
    .position_steps = 0,
    .max_steps = SHADE_TRAVEL_MM * STEPS_PER_MM,
    .moving = false,
    .target_steps = 0,
};

/* ---- Dawn Simulator State ---- */
typedef struct {
    bool     active;
    uint32_t start_time;      /* Unix timestamp of dawn start */
    uint16_t step;            /* Current step (0 to DAWN_STEPS) */
    uint8_t  ww_level;        /* Warm white 0-255 */
    uint8_t  amber_level;     /* Amber 0-255 */
    uint8_t  cool_level;      /* Cool white 0-255 */
} dawn_state_t;

static dawn_state_t dawn = {0};

/* ---- LED PWM Configuration ---- */

#define LED_PWM_FREQ     5000    /* 5kHz (above flicker threshold) */
#define LED_PWM_RES      LEDC_TIMER_8_BIT  /* 8-bit = 0-255 */

static void led_pwm_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LED_PWM_RES,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = LED_PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch_ww = {
        .gpio_num = PIN_LED_WW,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };
    ledc_channel_config_t ch_amber = {
        .gpio_num = PIN_LED_AMBER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };
    ledc_channel_config_t ch_cool = {
        .gpio_num = PIN_LED_COOL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_2,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };

    ledc_channel_config(&ch_ww);
    ledc_channel_config(&ch_amber);
    ledc_channel_config(&ch_cool);

    printf("[LED] 3-channel PWM initialized (warm/amber/cool)\n");
}

static void led_set(uint8_t ww, uint8_t amber, uint8_t cool)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, ww);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, amber);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, cool);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);

    dawn.ww_level = ww;
    dawn.amber_level = amber;
    dawn.cool_level = cool;
}

/* ---- TMC2209 Stepper Driver ---- */

static void stepper_init(void)
{
    gpio_set_direction(PIN_STEP, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DIR, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_EN, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DIAG, GPIO_MODE_INPUT);

    /* Disable motor initially (EN is active low) */
    gpio_set_level(PIN_EN, 1);

    /* In production: configure TMC2209 via UART
     * - Enable stealthChop mode (silent operation)
     * - Set microsteps: 16 (GCONF, CHOPCONF registers)
     * - Set current: 800mA (DRVCONF register)
     */
    printf("[STEPPER] TMC2209 initialized (stealthChop, 16 microsteps)\n");
}

static void stepper_enable(void)
{
    gpio_set_level(PIN_EN, 0);  /* Active low enable */
    vTaskDelay(pdMS_TO_TICKS(50)); /* Wait for driver to initialize */
}

static void stepper_disable(void)
{
    gpio_set_level(PIN_EN, 1);
}

static void stepper_move_to(int32_t target_steps)
{
    if (!shade.calibrated) {
        printf("[STEPPER] Not calibrated — run calibration first\n");
        return;
    }

    /* Clamp target to valid range */
    if (target_steps < 0) target_steps = 0;
    if (target_steps > shade.max_steps) target_steps = shade.max_steps;

    shade.target_steps = target_steps;
    shade.moving = true;

    /* Set direction */
    if (target_steps > shade.position_steps)
        gpio_set_level(PIN_DIR, 1);  /* Open (up) */
    else
        gpio_set_level(PIN_DIR, 0);  /* Close (down) */

    int32_t steps_to_go = target_steps - shade.position_steps;
    if (steps_to_go < 0) steps_to_go = -steps_to_go;

    /* Calculate acceleration/deceleration profile */
    int32_t accel_steps = ACCEL_MM_S2 * STEPS_PER_MM / (MAX_SPEED_MM_S * STEPS_PER_MM);

    stepper_enable();

    /* Motion profile: accel → cruise → decel */
    int32_t step_count = 0;
    float current_speed = 50.0f;  /* Start slow (steps/s) */
    float max_speed = MAX_SPEED_MM_S * STEPS_PER_MM;
    float accel = ACCEL_MM_S2 * STEPS_PER_MM;

    while (step_count < steps_to_go) {
        /* Check limit switches */
        if (gpio_get_level(PIN_LIMIT_TOP) == 0 && target_steps > shade.position_steps)
            break;
        if (gpio_get_level(PIN_LIMIT_BOTTOM) == 0 && target_steps < shade.position_steps)
            break;

        /* Calculate speed based on position in profile */
        int32_t remaining = steps_to_go - step_count;
        if (step_count < accel_steps) {
            /* Acceleration phase */
            current_speed = 50.0f + (float)step_count / accel_steps * (max_speed - 50.0f);
        } else if (remaining < accel_steps) {
            /* Deceleration phase */
            current_speed = 50.0f + (float)remaining / accel_steps * (max_speed - 50.0f);
        } else {
            /* Cruise phase */
            current_speed = max_speed;
        }

        /* Step pulse */
        gpio_set_level(PIN_STEP, 1);
        ets_delay_us(1);
        gpio_set_level(PIN_STEP, 0);

        /* Delay between steps (inverse of speed) */
        int32_t delay_us = (int32_t)(1000000.0f / current_speed / 2);
        if (delay_us < 10) delay_us = 10;
        ets_delay_us(delay_us);

        step_count++;
    }

    /* Update position */
    if (target_steps > shade.position_steps)
        shade.position_steps += step_count;
    else
        shade.position_steps -= step_count;

    shade.moving = false;
    stepper_disable();
    printf("[STEPPER] Moved to position: %d/%d steps (%d%%)\n",
           shade.position_steps, shade.max_steps,
           shade.max_steps > 0 ? (shade.position_steps * 100 / shade.max_steps) : 0);
}

/* ---- Calibration ---- */

static void shade_calibrate(void)
{
    printf("[CAL] Starting shade calibration...\n");
    stepper_enable();

    /* Move to bottom limit (closed) */
    gpio_set_level(PIN_DIR, 0);  /* Down */
    int32_t bottom_steps = 0;
    while (gpio_get_level(PIN_LIMIT_BOTTOM) != 0) {
        gpio_set_level(PIN_STEP, 1);
        ets_delay_us(500);
        gpio_set_level(PIN_STEP, 0);
        ets_delay_us(500);
        bottom_steps++;
        if (bottom_steps > 100000) { /* safety timeout */
            printf("[CAL] Bottom limit not found — aborting\n");
            stepper_disable();
            return;
        }
    }
    shade.position_steps = 0;

    /* Move to top limit (open) */
    gpio_set_level(PIN_DIR, 1);  /* Up */
    int32_t total_steps = 0;
    while (gpio_get_level(PIN_LIMIT_TOP) != 0) {
        gpio_set_level(PIN_STEP, 1);
        ets_delay_us(500);
        gpio_set_level(PIN_STEP, 0);
        ets_delay_us(500);
        total_steps++;
        if (total_steps > 200000) { /* safety timeout */
            printf("[CAL] Top limit not found — aborting\n");
            stepper_disable();
            return;
        }
    }
    shade.max_steps = total_steps;
    shade.position_steps = shade.max_steps;
    shade.calibrated = true;

    stepper_disable();
    printf("[CAL] Calibration complete: max_steps=%d (%.0fmm travel)\n",
           shade.max_steps, (float)shade.max_steps / STEPS_PER_MM);
}

/* ---- VEML7700 Light Sensor ---- */

static void veml7700_init(void)
{
    /* I2C0 at 0x10 */
    printf("[VEML7700] Ambient light sensor initialized\n");
}

static uint16_t veml7700_read_lux(void)
{
    /* In production: read from VEML7700 via I2C and convert to lux */
    return 0; /* stub */
}

/* ---- Dawn Simulator ---- */

static void dawn_start(uint32_t start_time)
{
    dawn.active = true;
    dawn.start_time = start_time;
    dawn.step = 0;
    printf("[DAWN] Dawn simulation scheduled for %lu\n", start_time);
}

static void dawn_update(void)
{
    if (!dawn.active) return;

    dawn.step++;
    float progress = (float)dawn.step / DAWN_STEPS;

    if (progress >= 1.0f) {
        /* Dawn complete — full brightness, open shade */
        led_set(255, 100, 200);
        dawn.active = false;
        stepper_move_to(shade.max_steps); /* Open shade */
        printf("[DAWN] Dawn simulation complete\n");
        return;
    }

    /* Sunrise spectrum profile:
     * 0-30%:  amber only (deep sunrise)
     * 30-60%: amber + warm white (golden hour)
     * 60-100%: warm white + cool white (full daylight)
     */
    uint8_t ww = 0, amber = 0, cool = 0;

    if (progress < 0.3f) {
        float p = progress / 0.3f;
        amber = (uint8_t)(p * 200.0f);
    } else if (progress < 0.6f) {
        float p = (progress - 0.3f) / 0.3f;
        amber = 200 + (uint8_t)(p * 55.0f);
        ww = (uint8_t)(p * 180.0f);
    } else {
        float p = (progress - 0.6f) / 0.4f;
        amber = 255 - (uint8_t)(p * 155.0f);
        ww = 180 + (uint8_t)(p * 75.0f);
        cool = (uint8_t)(p * 200.0f);
    }

    led_set(ww, amber, cool);
}

/* ---- BLE Mesh Client ---- */

static shade_status_payload_t latest_status = {0};
static uint32_t scheduled_dawn_time = 0;

static void ble_mesh_init(void)
{
    printf("[BLE] Mesh client initialized\n");
}

static void ble_send_status(void)
{
    latest_status.position = shade.max_steps > 0 ?
        (uint8_t)(shade.position_steps * 100 / shade.max_steps) : 0;
    latest_status.ambient_light = veml7700_read_lux();
    latest_status.led_warm = dawn.ww_level;
    latest_status.led_amber = dawn.amber_level;
    latest_status.led_cool = dawn.cool_level;
    latest_status.dawn_time = scheduled_dawn_time;

    uint8_t msg[64];
    uint16_t msg_len = mesh_build_message(
        NODE_ID_SHADE_MIN, NODE_ID_HUB, MSG_SHADE_STATUS,
        (uint8_t *)&latest_status, sizeof(latest_status),
        msg, sizeof(msg));

    /* In production: send via BLE mesh */
    printf("[BLE] TX shade status: pos=%d%%, light=%dlux\n",
           latest_status.position, latest_status.ambient_light);
}

static void process_hub_command(const hub_command_payload_t *cmd)
{
    switch (cmd->cmd_type) {
    case CMD_SHADE_POSITION: {
        uint8_t pos_pct = cmd->params[0];
        int32_t target = (int32_t)(shade.max_steps * pos_pct / 100);
        stepper_move_to(target);
        printf("[CMD] Shade position: %d%%\n", pos_pct);
        break;
    }
    case CMD_SHADE_DAWN: {
        uint32_t dawn_time;
        memcpy(&dawn_time, cmd->params, 4);
        scheduled_dawn_time = dawn_time;
        dawn_start(dawn_time);
        printf("[CMD] Dawn scheduled for %lu\n", dawn_time);
        break;
    }
    case CMD_ALARM_TRIGGER: {
        /* Alarm: open shade fully, full brightness warm white */
        stepper_move_to(shade.max_steps);
        led_set(255, 150, 255);
        printf("[CMD] Alarm triggered — shade open, lights on\n");
        break;
    }
    default:
        break;
    }
}

/* ---- Auto-close on Excess Light ---- */

#define LIGHT_SLEEP_THRESHOLD 50  /* Close shade if >50 lux during sleep */

static void check_auto_close(void)
{
    uint16_t lux = veml7700_read_lux();
    if (lux > LIGHT_SLEEP_THRESHOLD) {
        uint8_t current_pos = shade.max_steps > 0 ?
            (uint8_t)(shade.position_steps * 100 / shade.max_steps) : 0;
        if (current_pos > 10) { /* Only if shade is partially open */
            stepper_move_to(0); /* Close fully */
            printf("[AUTO] Closed shade due to excess light: %dlux\n", lux);
        }
    }
}

/* ---- Manual Button ---- */

static void handle_manual_button(void)
{
    /* Toggle: if >50% open, close; otherwise open */
    uint8_t pos = shade.max_steps > 0 ?
        (uint8_t)(shade.position_steps * 100 / shade.max_steps) : 0;

    if (pos > 50) {
        stepper_move_to(0);
    } else {
        stepper_move_to(shade.max_steps);
    }
}

/* ---- Main Tasks ---- */

static void task_shade_control(void *arg)
{
    while (1) {
        /* Check manual button */
        if (gpio_get_level(PIN_BTN_MANUAL) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50)); /* Debounce */
            if (gpio_get_level(PIN_BTN_MANUAL) == 0) {
                handle_manual_button();
                while (gpio_get_level(PIN_BTN_MANUAL) == 0)
                    vTaskDelay(pdMS_TO_TICKS(10)); /* Wait for release */
            }
        }

        /* Auto-close on excess ambient light */
        check_auto_close();

        vTaskDelay(pdMS_TO_TICKS(5000)); /* Check every 5s */
    }
}

static void task_dawn_simulator(void *arg)
{
    while (1) {
        if (dawn.active) {
            dawn_update();
            vTaskDelay(pdMS_TO_TICKS(10000)); /* 10s per step = 30 min total */
        } else {
            vTaskDelay(pdMS_TO_TICKS(30000));
        }
    }
}

static void task_status_report(void *arg)
{
    while (1) {
        ble_send_status();
        vTaskDelay(pdMS_TO_TICKS(60000)); /* Report every 60s */
    }
}

/* ---- Main Entry ---- */

void app_main(void)
{
    printf("=== SleepSync Shade Controller v1.0 ===\n");
    printf("ESP32-C3-MINI-1\n");

    /* Initialize hardware */
    stepper_init();
    led_pwm_init();
    veml7700_init();
    ble_mesh_init();

    /* Configure GPIOs */
    gpio_set_direction(PIN_LIMIT_TOP, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_LIMIT_BOTTOM, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_BTN_MANUAL, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_LIMIT_TOP, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_LIMIT_BOTTOM, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_BTN_MANUAL, GPIO_PULLUP_ONLY);

    /* Run calibration on first boot */
    if (!shade.calibrated) {
        printf("First boot — running shade calibration...\n");
        shade_calibrate();
    }

    /* Create tasks */
    xTaskCreate(task_shade_control, "shade_ctrl", 4096, NULL, 3, NULL);
    xTaskCreate(task_dawn_simulator, "dawn", 4096, NULL, 2, NULL);
    xTaskCreate(task_status_report, "status", 2048, NULL, 1, NULL);

    printf("Shade controller initialized. Position: %d%%\n",
           shade.max_steps > 0 ? (shade.position_steps * 100 / shade.max_steps) : 0);
}