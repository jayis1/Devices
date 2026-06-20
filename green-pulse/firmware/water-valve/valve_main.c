/*
 * valve_main.c — GreenPulse Water Valve Node firmware (ESP32-C6, ESP-IDF)
 *
 * Receives watering commands from the hub over Sub-GHz (SX1262), opens the
 * latching solenoid valve for the specified duration, monitors the flow
 * sensor (YF-S401) to confirm delivery and detect leaks/empty reservoir,
 * and reports the result back to the hub. Safety: auto-close after max
 * duration (10 min) and never open if pressure sensor reports zero.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/pcnt.h"  /* pulse counter for flow sensor */
#include "green_protocol.h"

static const char *TAG = "water_valve";

/* ---- Hardware pins ---- */
#define PIN_VALVE_OPEN    2    /* latching solenoid: open pulse */
#define PIN_VALVE_CLOSE   3    /* latching solenoid: close pulse */
#define PIN_FLOW_PULSE    4    /* YF-S401 hall-effect output */
#define PIN_PRESSURE_ADC  5    /* MPX5010DP pressure sensor */
#define PIN_LED_STATUS    6
#define PIN_SX1262_CS     7
#define PIN_SX1262_IRQ    8

/* ---- Flow sensor calibration (YF-S401) ---- */
/* F = flow_rate (L/min), pulses = flow sensor pulses
 * YF-S401: ~5880 pulses per liter (approx 98 pulses/sec at 1 L/min) */
#define PULSES_PER_LITER 5880.0f
#define MAX_DURATION_S   600   /* 10 min safety max */

/* ---- Valve zone (set via DIP switch or pairing) ---- */
static uint8_t zone_id = 0;

/* ---- Pulse counter for flow sensor ---- */
static int16_t flow_pulses = 0;

static void flow_pcnt_init(void)
{
    /* In production: configure PCNT unit 0, channel 0 on PIN_FLOW_PULSE,
     * count rising edges, filter glitches. */
    pcnt_config_t pcnt_cfg = {0};
    pcnt_cfg.pulse_gpio_num = PIN_FLOW_PULSE;
    pcnt_cfg.channel = PCNT_CHANNEL_0;
    pcnt_cfg.unit = PCNT_UNIT_0;
    pcnt_cfg.pos_mode = PCNT_COUNT_INC;
    pcnt_cfg.lctrl_mode = PCNT_MODE_KEEP;
    pcnt_cfg.hctrl_mode = PCNT_MODE_KEEP;
    /* pcnt_unit_config(&pcnt_cfg); */
    /* pcnt_counter_clear(PCNT_UNIT_0); */
    /* pcnt_counter_resume(PCNT_UNIT_0); */
}

static int16_t flow_pcnt_read_and_reset(void)
{
    int16_t count = flow_pulses;
    flow_pulses = 0;
    /* In production: pcnt_get_counter_value(PCNT_UNIT_0, &count); pcnt_counter_clear */
    return count;
}

/* ---- Latching solenoid control ---- */
static void valve_open(void)
{
    /* Latching solenoid: pulse OPEN pin for 50ms to open */
    gpio_set_level(PIN_VALVE_OPEN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_VALVE_OPEN, 0);
    ESP_LOGI(TAG, "Valve OPEN");
}

static void valve_close(void)
{
    /* Latching solenoid: pulse CLOSE pin for 50ms to close */
    gpio_set_level(PIN_VALVE_CLOSE, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_VALVE_CLOSE, 0);
    ESP_LOGI(TAG, "Valve CLOSED");
}

/* ---- Pressure sensor read (empty reservoir detection) ---- */
static float read_pressure_psi(void)
{
    /* MPX5010DP: Vout = Vcc * (0.018 * P + 0.04)
     * P in kPa. Read ADC, convert. */
    /* Stub: assume 14 PSI (normal) */
    return 14.0f;
}

/* ---- Execute watering command ---- */
static void execute_watering(uint8_t emitter, uint16_t duration_s,
                              uint16_t target_ml)
{
    uint16_t effective_duration = duration_s;
    if (effective_duration > MAX_DURATION_S)
        effective_duration = MAX_DURATION_S;

    /* Safety: check pressure (empty reservoir) */
    float pressure = read_pressure_psi();
    if (pressure < 1.0f) {
        ESP_LOGW(TAG, "No water pressure — reservoir empty!");
        gp_send_watering_ack(zone_id, GP_WATER_NO_FLOW, 0, 0, 0);
        return;
    }

    /* Clear flow counter */
    flow_pcnt_read_and_reset();

    /* Open valve */
    valve_open();
    gpio_set_level(PIN_LED_STATUS, 1);

    /* Wait for duration, monitoring flow */
    uint32_t elapsed_ms = 0;
    uint32_t target_ms = (uint32_t)effective_duration * 1000;
    while (elapsed_ms < target_ms) {
        vTaskDelay(pdMS_TO_TICKS(100));
        elapsed_ms += 100;
        /* Could check flow rate mid-watering for early leak/blockage detection */
    }

    /* Close valve */
    valve_close();
    gpio_set_level(PIN_LED_STATUS, 0);

    /* Read total flow */
    int16_t pulses = flow_pcnt_read_and_reset();
    uint16_t ml_delivered = (uint16_t)((float)pulses / PULSES_PER_LITER * 1000.0f);

    /* Leak detection: wait 3 seconds, check for residual flow */
    vTaskDelay(pdMS_TO_TICKS(3000));
    int16_t post_pulses = flow_pcnt_read_and_reset();
    uint8_t flags = 0;
    uint8_t status = GP_WATER_OK;

    if (post_pulses > 5) {
        /* Flow detected after close = leak */
        flags |= GP_ALERT_LEAK;
        status = GP_WATER_LEAK;
        ESP_LOGW(TAG, "LEAK detected: %d pulses after close", post_pulses);
    } else if (ml_delivered < (target_ml / 4) && target_ml > 0) {
        /* Delivered far less than target = empty or blocked */
        status = GP_WATER_NO_FLOW;
        ESP_LOGW(TAG, "Low flow: delivered %u/%u ml", ml_delivered, target_ml);
    }

    ESP_LOGI(TAG, "Watering done: emitter=%u duration=%us ml=%u status=%u",
             emitter, effective_duration, ml_delivered, status);

    /* Send ack to hub */
    gp_send_watering_ack(zone_id, status, ml_delivered, effective_duration, flags);
}

/* ---- Sub-GHz mesh RX ---- */
static void mesh_rx_handler(uint8_t type, const uint8_t *data, size_t len)
{
    if (type == GP_MSG_WATERING_CMD) {
        if (len < sizeof(gp_watering_cmd_payload_t) - 2) return;
        const gp_watering_cmd_payload_t *p = (const gp_watering_cmd_payload_t *)data;
        /* Check if this command is for our zone */
        if ((p->node_id & 0x07) != zone_id && (p->node_id & 0x07) != 0x07)
            return;  /* not our zone (0x07 = broadcast to all zones) */

        execute_watering(p->emitter_id, p->duration_s, p->target_ml);
    }
}

/* ---- SX1262 Sub-GHz radio init ---- */
static void sx1262_init(void)
{
    /* In production: SPI init, SX1262 register config for RX,
     * set frequency (868/915 MHz), sync word, CRC, RX duty cycle. */
}

static void sx1262_poll_rx(void)
{
    /* In production: check SX1262 IRQ pin, if DIO0 high read FIFO,
     * call gp_mesh_on_rx(buf, len). */
}

/* ---- Main ---- */
void app_main(void)
{
    /* Initialize GPIO */
    gpio_set_direction(PIN_VALVE_OPEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_VALVE_CLOSE, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LED_STATUS, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_VALVE_OPEN, 0);
    gpio_set_level(PIN_VALVE_CLOSE, 0);
    gpio_set_level(PIN_LED_STATUS, 0);

    /* Initialize flow pulse counter */
    flow_pcnt_init();

    /* Initialize Sub-GHz radio */
    sx1262_init();
    gp_mesh_set_rx_callback(mesh_rx_handler);

    /* Ensure valve is closed at boot (safety) */
    valve_close();

    ESP_LOGI(TAG, "GreenPulse Water Valve node started (zone=%u)", zone_id);

    while (1) {
        sx1262_poll_rx();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}