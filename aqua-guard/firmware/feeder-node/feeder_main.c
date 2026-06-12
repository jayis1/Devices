/*
 * feeder_main.c — Aqua Guard Feeder Node (ESP32-S3)
 *
 * Rim-mounted actuator: peristaltic pumps, servo feeder, RGBW LED, camera.
 * Receives commands from hub over Sub-GHz mesh.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "driver/spi.h"
#include "esp_camera.h"
#include "nvs_flash.h"

#include "mesh_protocol.h"

static const char *TAG = "FEEDER";

/* ---- Pin Definitions ---- */
#define PIN_SX1261_CS      1
#define PIN_PUMP1_PWM      18
#define PIN_PUMP2_PWM      19
#define PIN_PUMP3_PWM      20
#define PIN_PUMP4_PWM      21
#define PIN_PUMP5_PWM      26
#define PIN_PUMP6_PWM      27
#define PIN_SERVO_PWM      17
#define PIN_LED_R          32
#define PIN_LED_G          33
#define PIN_LED_B          34
#define PIN_LED_W          35
#define PIN_HOPPER_IR      36

/* ---- Peristaltic Pump Control ---- */

typedef struct {
    gpio_num_t pwm_pin;
    float ml_per_min;    /* calibrated flow rate */
    bool running;
    uint8_t ledc_channel;
} pump_t;

static pump_t pumps[6] = {
    {GPIO_NUM_18, 1.5f, false, 0},  /* Dechlorinator */
    {GPIO_NUM_19, 1.2f, false, 1},  /* pH Buffer */
    {GPIO_NUM_20, 0.8f, false, 2},  /* Fertilizer */
    {GPIO_NUM_21, 1.0f, false, 3},  /* Medication A */
    {GPIO_NUM_26, 1.0f, false, 4},  /* Medication B */
    {GPIO_NUM_27, 0.5f, false, 5},  /* Buffer/Calibration */
};

static void pumps_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 25000,  /* 25kHz for DRV8833 */
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    for (int i = 0; i < 6; i++) {
        ledc_channel_config_t ch_conf = {
            .gpio_num = pumps[i].pwm_pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = pumps[i].ledc_channel,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
        };
        ledc_channel_config(&ch_conf);
    }
    ESP_LOGI(TAG, "6 peristaltic pumps initialized");
}

static void pump_dose_ml(uint8_t pump_id, float volume_ml)
{
    if (pump_id >= 6) return;

    pump_t *p = &pumps[pump_id];
    float time_seconds = (volume_ml / p->ml_per_min) * 60.0f;

    ESP_LOGI(TAG, "Pump %d: dosing %.2f mL (%.1f seconds at %.1f mL/min)",
             pump_id, volume_ml, time_seconds, p->ml_per_min);

    /* Run pump at full speed */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, p->ledc_channel, 1023);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, p->ledc_channel);
    p->running = true;

    /* Wait for calculated time */
    vTaskDelay(pdMS_TO_TICKS((uint32_t)(time_seconds * 1000)));

    /* Stop pump */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, p->ledc_channel, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, p->ledc_channel);
    p->running = false;

    ESP_LOGI(TAG, "Pump %d: dose complete", pump_id);
}

/* ---- Servo Feeder ---- */

static void feeder_servo_init(void)
{
    ledc_channel_config_t servo_conf = {
        .gpio_num = PIN_SERVO_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = 6,
        .timer_sel = LEDC_TIMER_1,
        .duty_resolution = LEDC_TIMER_15_BIT,
        .duty = 0,
    };
    ledc_timer_config_t servo_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_15_BIT,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = 50,  /* 50Hz servo */
    };
    ledc_timer_config(&servo_timer);
    ledc_channel_config(&servo_conf);
}

static void feeder_dispense(uint8_t portions)
{
    ESP_LOGI(TAG, "Feeding %d portion(s)", portions);
    for (uint8_t p = 0; p < portions; p++) {
        /* Open hopper: servo to 180° (duty ~6.4% = 2090/32768) */
        ledc_set_duty(LEDC_LOW_SPEED_MODE, 6, 2090);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, 6);
        vTaskDelay(pdMS_TO_TICKS(2000));

        /* Close hopper: servo to 0° (duty ~2.5% = 820/32768) */
        ledc_set_duty(LEDC_LOW_SPEED_MODE, 6, 820);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, 6);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "Feeding complete");
}

/* ---- RGBW LED Control ---- */

static uint8_t current_led[4] = {0};  /* R, G, B, W */

static void leds_init(void)
{
    gpio_num_t led_pins[4] = {GPIO_NUM_32, GPIO_NUM_33, GPIO_NUM_34, GPIO_NUM_35};
    for (int i = 0; i < 4; i++) {
        ledc_channel_config_t ch = {
            .gpio_num = led_pins[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = 7 + i,
            .timer_sel = LEDC_TIMER_2,
            .duty_resolution = LEDC_TIMER_10_BIT,
            .duty = 0,
        };
        ledc_channel_config(&ch);
    }
    ledc_timer_config_t led_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_2,
        .freq_hz = 20000,  /* 20kHz for AL8860 */
    };
    ledc_timer_config(&led_timer);
}

static void leds_set(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    current_led[0] = r;
    current_led[1] = g;
    current_led[2] = b;
    current_led[3] = w;

    uint16_t duties[4] = {
        (uint16_t)((r / 255.0f) * 1023),
        (uint16_t)((g / 255.0f) * 1023),
        (uint16_t)((b / 255.0f) * 1023),
        (uint16_t)((w / 255.0f) * 1023),
    };

    for (int i = 0; i < 4; i++) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, 7 + i, duties[i]);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, 7 + i);
    }
}

/* ---- Circadian Lighting Schedule ---- */

typedef struct {
    uint8_t hour;
    uint8_t r, g, b, w;
} circadian_point_t;

/* Tropical freshwater schedule */
static const circadian_point_t tropical_schedule[] = {
    { 0,   0,   0,   0,   0  },  /* Midnight: off */
    { 5,   0,   0,   10,  5  },  /* 5AM: moonlight */
    { 6,   40,  20,  10,  30 },  /* 6AM: dawn */
    { 7,   120, 80,  40,  80 },  /* 7AM: sunrise */
    { 8,   200, 180, 100, 150},  /* 8AM: morning */
    {10,   255, 220, 150, 200},  /* 10AM: full daylight */
    {14,   255, 200, 120, 180},  /* 2PM: afternoon */
    {17,   200, 120, 60,  120},  /* 5PM: late afternoon */
    {18,   120, 60,  30,  60 },  /* 6PM: sunset */
    {19,   40,  15,  20,  20 },  /* 7PM: dusk */
    {20,   5,   0,   10,  5  },  /* 8PM: moonlight */
    {21,   0,   0,   0,   0  },  /* 9PM: lights out */
};

#define SCHEDULE_POINTS (sizeof(tropical_schedule) / sizeof(circadian_point_t))

static void apply_circadian(void)
{
    /* Get current hour from RTC/NTP (stub: use frame counter) */
    uint8_t hour = 10;  /* stub */

    /* Find surrounding schedule points and interpolate */
    int lo = 0, hi = 1;
    for (int i = 0; i < (int)SCHEDULE_POINTS - 1; i++) {
        if (tropical_schedule[i].hour <= hour && tropical_schedule[i + 1].hour > hour) {
            lo = i;
            hi = i + 1;
            break;
        }
    }

    /* Linear interpolation */
    float frac = (hour - tropical_schedule[lo].hour) /
                 (float)(tropical_schedule[hi].hour - tropical_schedule[lo].hour);

    uint8_t r = tropical_schedule[lo].r + frac * (tropical_schedule[hi].r - tropical_schedule[lo].r);
    uint8_t g = tropical_schedule[lo].g + frac * (tropical_schedule[hi].g - tropical_schedule[lo].g);
    uint8_t b = tropical_schedule[lo].b + frac * (tropical_schedule[hi].b - tropical_schedule[lo].b);
    uint8_t w = tropical_schedule[lo].w + frac * (tropical_schedule[hi].w - tropical_schedule[lo].w);

    leds_set(r, g, b, w);
}

/* ---- Command Processing ---- */

static void process_command(const command_payload_t *cmd)
{
    switch (cmd->cmd_type) {
    case CMD_DOSE: {
        uint8_t pump_id = cmd->params[0];
        uint16_t volume_ml = (cmd->params[1] << 8) | cmd->params[2];
        pump_dose_ml(pump_id, volume_ml / 10.0f);
        break;
    }
    case CMD_FEED: {
        uint8_t portions = cmd->params[0];
        feeder_dispense(portions);
        break;
    }
    case CMD_LIGHT: {
        uint8_t r = cmd->params[0];
        uint8_t g = cmd->params[1];
        uint8_t b = cmd->params[2];
        uint8_t w = cmd->params[3];
        leds_set(r, g, b, w);
        ESP_LOGI(TAG, "Manual light: R=%d G=%d B=%d W=%d", r, g, b, w);
        break;
    }
    default:
        ESP_LOGW(TAG, "Unknown command type: 0x%02X", cmd->cmd_type);
        break;
    }
}

/* ---- Mesh Radio (stub) ---- */

static void sx1261_init(void)
{
    /* SX1261 on SPI + I2C pins */
    ESP_LOGI(TAG, "SX1261 radio initialized (868MHz LoRa)");
}

static int16_t sx1261_receive(uint8_t *buf, uint16_t max_len)
{
    /* In production: poll IRQ, read FIFO on RxDone */
    return 0;
}

static void sx1261_send_status(const feeder_status_payload_t *status)
{
    mesh_packet_t tx_pkt;
    mesh_build_packet(NODE_ID_FEEDER, NODE_ID_HUB, PKT_FEEDER_STATUS,
                      (uint8_t *)status, sizeof(feeder_status_payload_t), &tx_pkt);
    /* sx1261_send((uint8_t *)&tx_pkt, pkt_len); */
}

/* ---- Main ---- */

void app_main(void)
{
    ESP_LOGI(TAG, "=== Aqua Guard Feeder Node v1.0 ===");

    nvs_flash_init();
    pumps_init();
    feeder_servo_init();
    leds_init();
    sx1261_init();

    ESP_LOGI(TAG, "All subsystems initialized");

    feeder_status_payload_t status = {0};
    uint32_t loop = 0;

    while (true) {
        /* Check for incoming commands from hub */
        mesh_packet_t rx_pkt;
        uint8_t rx_buf[64];
        int16_t rx_len = sx1261_receive(rx_buf, sizeof(rx_buf));
        if (rx_len > 0 && mesh_parse_packet(rx_buf, rx_len, &rx_pkt) == 0) {
            if (rx_pkt.dst_id == NODE_ID_FEEDER && rx_pkt.pkt_type == PKT_COMMAND) {
                command_payload_t cmd;
                memcpy(&cmd, rx_pkt.payload, sizeof(command_payload_t));
                process_command(&cmd);
            }
        }

        /* Update circadian lighting every 60 seconds */
        if (loop % 60 == 0) {
            apply_circadian();
        }

        /* Build status packet */
        status.pump_states = 0;
        for (int i = 0; i < 6; i++) {
            if (pumps[i].running) status.pump_states |= (1 << i);
        }
        status.led_r = current_led[0];
        status.led_g = current_led[1];
        status.led_b = current_led[2];
        status.led_w = current_led[3];

        /* Transmit status to hub in our TDMA slot */
        sx1261_send_status(&status);

        /* Main loop: 1 second cycle (aligned to TDMA frame) */
        vTaskDelay(pdMS_TO_TICKS(1000));
        loop++;
    }
}