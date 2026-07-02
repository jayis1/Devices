/**
 * JointSync Smart Compression Sleeve — Main Firmware
 *
 * ESP32-S3-MINI-1 + CC1120 Sub-GHz + micro-pump + BMP390 + NAU7802
 * Delivers adaptive pneumatic compression (20-40 mmHg).
 *
 * License: MIT
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "protocol.h"
#include "pressure_control.h"
#include "pump_driver.h"
#include "subghz_node.h"

static const char *TAG = "compression_sleeve";

/* ── State ───────────────────────────────────────────────────────── */

typedef enum {
    SLEEVE_STATE_IDLE = 0,
    SLEEVE_STATE_INFLATING,
    SLEEVE_STATE_HOLDING,
    SLEEVE_STATE_DEFLATING,
    SLEEVE_STATE_PULSED,
    SLEEVE_STATE_ERROR,
} sleeve_state_t;

typedef struct {
    sleeve_state_t state;
    uint8_t  target_mmhg;
    uint16_t duration_sec;
    uint8_t  mode;          /* 0=rest, 1=active, 2=pulsed, 3=adaptive */
    uint8_t  joint_id;
    uint16_t node_id;
    uint16_t seq_counter;
    float    current_pressure;
    int64_t  therapy_start_us;
    int64_t  therapy_end_us;
    bool     pump_on;
} sleeve_state_t_ext;

static sleeve_state_t_ext g_sleeve = {0};

/* ── Sub-GHz Command Handler ──────────────────────────────────────── */

static void subghz_cmd_handler(uint16_t sender_id, const uint8_t *data, uint8_t len)
{
    jointsync_header_t header;
    const uint8_t *payload;

    if (!jointsync_decode(data, len, &header, &payload)) {
        ESP_LOGW(TAG, "Invalid packet from 0x%04X", sender_id);
        return;
    }

    switch (header.msg_type) {
    case MSG_TYPE_CMD_THERAPY:
        if (header.payload_len >= sizeof(payload_therapy_t)) {
            payload_therapy_t *cmd = (payload_therapy_t *)payload;
            g_sleeve.mode = cmd->mode;
            g_sleeve.target_mmhg = cmd->target_mmhg;
            g_sleeve.duration_sec = cmd->duration_sec;
            g_sleeve.joint_id = cmd->joint_id;
            g_sleeve.therapy_start_us = esp_timer_get_time();
            g_sleeve.therapy_end_us = g_sleeve.therapy_start_us + (int64_t)cmd->duration_sec * 1000000;

            g_sleeve.state = SLEEVE_STATE_INFLATING;
            pump_driver_inflate();

            ESP_LOGI(TAG, "Therapy: mode=%d target=%d mmHg duration=%d sec joint=%d",
                     cmd->mode, cmd->target_mmhg, cmd->duration_sec, cmd->joint_id);
        }
        break;

    case MSG_TYPE_HEARTBEAT:
        ESP_LOGD(TAG, "Hub heartbeat received");
        break;

    default:
        break;
    }
}

/* ── Pressure Control Task ───────────────────────────────────────── */

static void pressure_control_task(void *arg)
{
    const TickType_t cycle = pdMS_TO_TICKS(100);  /* 100 ms control loop */
    float pid_output = 0;

    while (1) {
        /* Read current pressure from BMP390 + load cell */
        g_sleeve.current_pressure = pressure_control_read();

        int64_t now_us = esp_timer_get_time();

        switch (g_sleeve.state) {
        case SLEEVE_STATE_IDLE:
            /* Slowly deflate if any residual pressure */
            if (g_sleeve.current_pressure > 5.0f) {
                pump_driver_deflate();
            } else {
                pump_driver_off();
            }
            break;

        case SLEEVE_STATE_INFLATING:
            /* PID control to reach target pressure */
            pid_output = pressure_control_pid(g_sleeve.target_mmhg,
                                               g_sleeve.current_pressure);
            pump_driver_set_pwm(pid_output);

            if (g_sleeve.current_pressure >= g_sleeve.target_mmhg - 1.0f) {
                g_sleeve.state = SLEEVE_STATE_HOLDING;
                pump_driver_hold();
                ESP_LOGI(TAG, "Target pressure reached: %.1f mmHg", g_sleeve.current_pressure);
            }
            break;

        case SLEEVE_STATE_HOLDING:
            /* Maintain pressure with minor corrections */
            pid_output = pressure_control_pid(g_sleeve.target_mmhg,
                                               g_sleeve.current_pressure);
            if (pid_output > 0.1f) {
                pump_driver_inflate();
                pump_driver_set_pwm(pid_output * 0.5f);
            } else if (g_sleeve.current_pressure > g_sleeve.target_mmhg + 2.0f) {
                pump_driver_deflate();
            } else {
                pump_driver_hold();
            }

            /* Check duration */
            if (now_us >= g_sleeve.therapy_end_us) {
                if (g_sleeve.mode == 2) {  /* Pulsed mode */
                    /* Toggle between high and low pressure */
                    if (g_sleeve.target_mmhg > 30) {
                        g_sleeve.target_mmhg = 20;
                    } else {
                        g_sleeve.target_mmhg = 40;
                    }
                    g_sleeve.state = SLEEVE_STATE_INFLATING;
                    g_sleeve.therapy_end_us = now_us + 60000000;  /* 60 sec pulse */
                } else {
                    g_sleeve.state = SLEEVE_STATE_DEFLATING;
                    pump_driver_deflate();
                    ESP_LOGI(TAG, "Therapy complete, deflating");
                }
            }
            break;

        case SLEEVE_STATE_DEFLATING:
            pump_driver_deflate();
            if (g_sleeve.current_pressure < 3.0f) {
                g_sleeve.state = SLEEVE_STATE_IDLE;
                pump_driver_off();
                ESP_LOGI(TAG, "Deflation complete, idle");
            }
            break;

        case SLEEVE_STATE_ERROR:
            pump_driver_off();
            break;

        default:
            break;
        }

        /* Send pressure data to Hub every 500 ms */
        static int report_counter = 0;
        if (++report_counter >= 5) {  /* 5 × 100 ms = 500 ms */
            report_counter = 0;

            payload_pressure_t payload;
            payload.pressure_centi = (int16_t)(g_sleeve.current_pressure * 100.0f);
            payload.load_raw = pressure_control_read_loadcell();
            payload.timestamp = (uint32_t)(now_us / 1000);
            payload.pump_state = (uint8_t)g_sleeve.state;

            uint8_t packet[JS_MAX_PACKET_LEN];
            uint8_t pkt_len = jointsync_encode(packet, sizeof(packet),
                                                MSG_TYPE_DATA_PRESSURE,
                                                g_sleeve.node_id,
                                                g_sleeve.seq_counter++,
                                                0,
                                                (uint8_t *)&payload, sizeof(payload));
            subghz_node_send(packet, pkt_len);
        }

        vTaskDelay(cycle);
    }
}

/* ── Manual Button Task ──────────────────────────────────────────── */

static void button_task(void *arg)
{
    static bool prev_btn = false;

    while (1) {
        bool btn = (gpio_get_level(13) == 0);  /* Active low */

        if (btn && !prev_btn && g_sleeve.state == SLEEVE_STATE_IDLE) {
            /* Manual inflate to 30 mmHg for 30 minutes */
            g_sleeve.mode = 0;
            g_sleeve.target_mmhg = 30;
            g_sleeve.duration_sec = 1800;
            g_sleeve.joint_id = 0;
            g_sleeve.therapy_start_us = esp_timer_get_time();
            g_sleeve.therapy_end_us = g_sleeve.therapy_start_us + 1800000000;
            g_sleeve.state = SLEEVE_STATE_INFLATING;
            pump_driver_inflate();
            ESP_LOGI(TAG, "Manual therapy button pressed");
        }

        prev_btn = btn;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ── Main ────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "JointSync Compression Sleeve starting...");

    /* Initialize state */
    g_sleeve.state = SLEEVE_STATE_IDLE;
    g_sleeve.node_id = 0x0100;  /* Sleeve base ID */
    g_sleeve.seq_counter = 0;
    g_sleeve.target_mmhg = 0;
    g_sleeve.current_pressure = 0;
    g_sleeve.pump_on = false;

    /* Initialize GPIO */
    gpio_set_direction(13, GPIO_MODE_INPUT);  /* Button */
    gpio_pullup_en(13);
    gpio_set_direction(14, GPIO_MODE_OUTPUT); /* Status LED */

    /* Initialize pump driver (PWM + valve control) */
    pump_driver_init();

    /* Initialize pressure sensor (BMP390 + NAU7802) */
    pressure_control_init();

    /* Initialize Sub-GHz node (CC1120) */
    subghz_node_init(subghz_cmd_handler);
    subghz_node_start_rx();

    /* Create tasks */
    xTaskCreate(pressure_control_task, "pressure_ctrl", 4096, NULL, 5, NULL);
    xTaskCreate(button_task, "button", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "Sleeve ready (node 0x%04X)", g_sleeve.node_id);

    /* Main loop — heartbeat */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        /* Send heartbeat */
        payload_status_t status;
        status.battery_pct = 100;  /* TODO: read LiPo voltage */
        status.state = (uint8_t)g_sleeve.state;
        status.error_code = 0;

        uint8_t packet[JS_MAX_PACKET_LEN];
        uint8_t pkt_len = jointsync_encode(packet, sizeof(packet),
                                            MSG_TYPE_HEARTBEAT,
                                            g_sleeve.node_id,
                                            g_sleeve.seq_counter++,
                                            0,
                                            (uint8_t *)&status, sizeof(status));
        subghz_node_send(packet, pkt_len);

        /* Blink status LED */
        gpio_set_level(14, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(14, 0);
    }
}