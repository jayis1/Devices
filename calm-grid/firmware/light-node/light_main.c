/*
 * light_main.c — CalmGrid Light Node firmware (ESP32-C6, ESP-IDF)
 *
 * Controls tunable-white LED lighting via the TLC5973 16-channel PWM
 * driver. Receives scene commands from the hub over BLE mesh and
 * maintains circadian schedules offline. Uses the TSL2591 ambient
 * light sensor for closed-loop brightness control.
 *
 * Scenes:
 *   0=off, 1=circadian, 2=work, 3=de-stress, 4=breathing,
 *   5=sunset, 6=sunrise
 *
 * Circadian schedule:
 *   - 06:00-09:00: sunrise (warm→cool, dim→bright)
 *   - 09:00-17:00: work (cool, bright)
 *   - 17:00-21:00: sunset (cool→warm, bright→dim)
 *   - 21:00-06:00: off (or very dim warm)
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "calm_protocol.h"

static const char *TAG = "calmgrid-light";

/* ---- Pin definitions (ESP32-C6) ---- */
#define LED_WARM_PIN    1   /* PWM for warm channel (2700K) */
#define LED_COOL_PIN    2   /* PWM for cool channel (6500K) */
#define TLC_SCK_PIN     3
#define TLC_DATA_PIN    4
#define TLC_LATCH_PIN   5
#define TSL2591_SDA     6
#define TSL2591_SCL     7
#define OLED_SDA_PIN    12
#define OLED_SCL_PIN    13
#define WS2812_PIN      14
#define BUTTON_PIN      15
#define OVERRIDE_PIN    16

/* ---- Lighting state ---- */
typedef struct {
    uint8_t  scene;
    uint8_t  brightness;     /* 0-100 % */
    uint16_t warm_kelvin;
    uint16_t cool_kelvin;
    uint8_t  current_warm;   /* actual PWM warm value */
    uint8_t  current_cool;   /* actual PWM cool value */
    uint16_t ambient_lux;    /* from TSL2591 */
    uint8_t  override_active;
    uint32_t last_scene_change_ms;
} light_state_t;

static light_state_t g_state = {
    .scene = CALM_SCENE_CIRCADIAN,
    .brightness = 80,
    .warm_kelvin = 2700,
    .cool_kelvin = 6500,
    .current_warm = 0,
    .current_cool = 0,
    .ambient_lux = 0,
    .override_active = 0,
    .last_scene_change_ms = 0,
};

/* ---- Scene definitions ---- */
typedef struct {
    uint8_t  warm_pwm;   /* 0-255 */
    uint8_t  cool_pwm;   /* 0-255 */
    const char *name;
} scene_def_t;

/* Base scene definitions at brightness=100% */
static const scene_def_t scene_defs[] = {
    {   0,   0, "Off" },              /* 0: off */
    { 128, 255, "Circadian" },        /* 1: circadian (variable) */
    {  32, 255, "Work" },             /* 2: work (cool, bright) */
    { 255,  32, "De-stress" },        /* 3: de-stress (warm, low) */
    { 180,  60, "Breathing" },        /* 4: breathing (warm pulse) */
    { 200,  16, "Sunset" },           /* 5: sunset (warm dim) */
    { 255, 255, "Sunrise" },          /* 6: sunrise (warm→cool) */
};

/* ---- I2C for TSL2591 + OLED ---- */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TSL2591_SDA,
        .scl_io_num = TSL2591_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

/* ---- Read TSL2591 ambient light (lux * 10) ---- */
static uint16_t tsl2591_read_lux(void)
{
    uint8_t data[2];
    /* Read channel 0 + 1 */
    i2c_master_write_read_device(I2C_NUM_0, 0x29,
                                 (uint8_t[]){0xA4 | 0x20}, 1, data, 2,
                                 100 / portTICK_PERIOD_MS);
    uint16_t ch0 = (data[1] << 8) | data[0];
    /* Simplified lux calculation (in production: use both channels) */
    return (uint16_t)(ch0 * 0.15f);  /* approximate lux * 10 */
}

/* ---- PWM output for warm + cool channels ---- */
static void pwm_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t warm_conf = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = LED_WARM_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
    };
    ledc_channel_config(&warm_conf);

    ledc_channel_config_t cool_conf = {
        .channel = LEDC_CHANNEL_1,
        .duty = 0,
        .gpio_num = LED_COOL_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
    };
    ledc_channel_config(&cool_conf);
}

static void set_led_output(uint8_t warm, uint8_t cool)
{
    /* Scale by brightness */
    uint8_t w = (uint16_t)warm * g_state.brightness / 100;
    uint8_t c = (uint16_t)cool * g_state.brightness / 100;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, w);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, c);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

    g_state.current_warm = w;
    g_state.current_cool = c;
}

/* ---- Apply a scene ---- */
static void apply_scene(uint8_t scene)
{
    if (scene > 6) scene = CALM_SCENE_CIRCADIAN;
    g_state.scene = scene;
    g_state.last_scene_change_ms = (uint32_t)(esp_timer_get_time() / 1000);

    const scene_def_t *def = &scene_defs[scene];
    set_led_output(def->warm_pwm, def->cool_pwm);

    ESP_LOGI(TAG, "Scene: %s (warm=%u cool=%u bright=%u%%)",
             def->name, g_state.current_warm, g_state.current_cool,
             g_state.brightness);
}

/* ---- Circadian schedule (simplified — uses NTP time in production) ---- */
/* In production, time comes from RTC or NTP via hub.
 * For now, uses a simple day-phase approximation. */
static uint8_t circadian_scene_for_hour(int hour)
{
    if (hour >= 6 && hour < 9) return CALM_SCENE_SUNRISE;
    if (hour >= 9 && hour < 17) return CALM_SCENE_WORK;
    if (hour >= 17 && hour < 21) return CALM_SCENE_SUNSET;
    return CALM_SCENE_OFF;
}

/* ---- Closed-loop brightness control via TSL2591 ---- */
static void brightness_control_task(void *arg)
{
    (void)arg;
    while (1) {
        /* Read ambient light */
        g_state.ambient_lux = tsl2591_read_lux();

        /* Closed-loop: if ambient is already bright, dim the LEDs
         * to maintain a target total lux level */
        uint16_t target_lux = 500;  /* target 500 lux for work scenes */
        if (g_state.scene == CALM_SCENE_DESTRESS)
            target_lux = 150;
        else if (g_state.scene == CALM_SCENE_SUNSET)
            target_lux = 100;
        else if (g_state.scene == CALM_SCENE_OFF)
            target_lux = 0;

        if (g_state.ambient_lux > target_lux + 100) {
            /* Too bright — reduce LED brightness */
            if (g_state.brightness > 20)
                g_state.brightness -= 5;
        } else if (g_state.ambient_lux < target_lux - 100) {
            /* Too dim — increase LED brightness */
            if (g_state.brightness < 100)
                g_state.brightness += 5;
        }

        /* Re-apply current scene with adjusted brightness */
        if (g_state.scene != CALM_SCENE_OFF) {
            const scene_def_t *def = &scene_defs[g_state.scene];
            set_led_output(def->warm_pwm, def->cool_pwm);
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);  /* update every 5s */
    }
}

/* ---- Scene management task (handles circadian + breathing pulse) ---- */
static void scene_task(void *arg)
{
    (void)arg;
    uint32_t breath_phase = 0;

    while (1) {
        /* Check override (wall switch) */
        if (gpio_get_level(OVERRIDE_PIN) == 0 && !g_state.override_active) {
            g_state.override_active = 1;
            /* Cycle through scenes on button press */
            static uint8_t scene_cycle = 1;
            scene_cycle = (scene_cycle % 4) + 1;  /* circadian → work → de-stress → off */
            apply_scene(scene_cycle);
        } else if (gpio_get_level(OVERRIDE_PIN) == 1) {
            g_state.override_active = 0;
        }

        /* Circadian auto-mode (if not overridden) */
        if (g_state.scene == CALM_SCENE_CIRCADIAN && !g_state.override_active) {
            /* In production: get hour from RTC/NTP
             * For now: use a placeholder hour */
            static int sim_hour = 8;
            uint8_t target = circadian_scene_for_hour(sim_hour);
            /* Don't switch to OFF automatically during circadian —
             * just adjust warm/cool ratio based on time of day */
            if (target == CALM_SCENE_SUNRISE) {
                set_led_output(255, 128);  /* warm with some cool */
            } else if (target == CALM_SCENE_WORK) {
                set_led_output(32, 255);   /* cool bright */
            } else if (target == CALM_SCENE_SUNSET) {
                set_led_output(200, 32);   /* warm dim */
            } else {
                set_led_output(0, 0);      /* off at night */
            }
        }

        /* Breathing scene: gentle pulse */
        if (g_state.scene == CALM_SCENE_BREATHING) {
            breath_phase++;
            /* 4-7-8 pattern: inhale 4s, hold 7s, exhale 8s → 19s cycle
             * At 100ms update → 190 steps per cycle */
            int cycle_pos = breath_phase % 190;
            uint8_t brightness;
            if (cycle_pos < 40) {
                /* Inhale — brighten */
                brightness = 20 + (cycle_pos * 2);  /* 20→100 */
            } else if (cycle_pos < 110) {
                /* Hold */
                brightness = 100;
            } else {
                /* Exhale — dim */
                brightness = 100 - ((cycle_pos - 110) * 1);  /* 100→20 */
            }
            g_state.brightness = brightness;
            set_led_output(180, 60);  /* warm tone */
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/* ---- BLE mesh RX handler ---- */
static void light_mesh_rx_handler(uint8_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case CALM_MSG_LIGHTING: {
        if (len < sizeof(calm_lighting_payload_t) - 2) break;
        const calm_lighting_payload_t *p = (const calm_lighting_payload_t *)data;
        apply_scene(p->scene);
        g_state.brightness = p->brightness;
        if (p->warm_kelvin > 0) g_state.warm_kelvin = p->warm_kelvin;
        if (p->cool_kelvin > 0) g_state.cool_kelvin = p->cool_kelvin;
        ESP_LOGI(TAG, "Received scene %u brightness %u%%", p->scene, p->brightness);
        break;
    }
    case CALM_MSG_STRESS_SCORE: {
        if (len < sizeof(calm_stress_score_payload_t) - 2) break;
        const calm_stress_score_payload_t *p = (const calm_stress_score_payload_t *)data;
        /* Autonomous response: if stress high, shift to de-stress */
        if (p->stress_score >= 70 && g_state.scene != CALM_SCENE_BREATHING) {
            apply_scene(CALM_SCENE_DESTRESS);
        }
        break;
    }
    case CALM_MSG_INTERVENTION: {
        if (len < sizeof(calm_intervention_payload_t) - 2) break;
        const calm_intervention_payload_t *p = (const calm_intervention_payload_t *)data;
        /* Lighting intervention */
        if (p->intervention_id == 2 || p->intervention_id == 4) {
            apply_scene(CALM_SCENE_BREATHING);
        }
        break;
    }
    default:
        break;
    }
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "CalmGrid Light Node starting...");

    /* Initialize GPIO */
    gpio_config_t input_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN) | (1ULL << OVERRIDE_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&input_conf);

    /* Initialize I2C + PWM */
    i2c_init();
    pwm_init();

    /* Set up mesh RX handler */
    calm_mesh_set_rx_callback(light_mesh_rx_handler);

    /* Apply initial scene */
    apply_scene(CALM_SCENE_CIRCADIAN);

    /* Create tasks */
    xTaskCreate(scene_task, "scene", 4096, NULL, 5, NULL);
    xTaskCreate(brightness_control_task, "brightness", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Light node ready. Scene: Circadian, Brightness: 80%%");
}