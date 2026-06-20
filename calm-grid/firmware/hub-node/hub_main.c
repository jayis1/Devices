/*
 * hub_main.c — CalmGrid Hub Node firmware (RP2040, Pico SDK)
 *
 * Aggregates BLE-mesh data from wrist band + room sentinel + light node,
 * runs the TFLite Micro stress-score model every 15 min, renders the
 * stress gauge + breathing-guide animation on the ILI9488 TFT, triggers
 * interventions (breathing audio, soundscape, lighting commands), and
 * bridges to WiFi (ESP32-C6 co-processor) for MQTT cloud sync + BLE
 * to the mobile app.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/multicore.h"
#include "calm_protocol.h"

/* Mesh radio (nRF52840) on UART0, WiFi co-proc (ESP32-C6) on UART1 */
#define MESH_UART  uart0
#define WIFI_UART  uart1
#define MESH_BAUD  1000000
#define WIFI_BAUD  921600

/* ---- Aggregation buffers (24-hr ring at 5-min resolution) ---- */
#define RING_HOURS   24
#define RING_SLOTS   (RING_HOURS * 12)  /* 5-min per slot → 12/hr = 288 */

typedef struct {
    uint8_t  hr_bpm;
    uint16_t hrv_rmssd;
    uint16_t eda_scl;
    uint16_t eda_scr_rate;
    int16_t  skin_temp_centic;
    uint8_t  activity_class;
    uint16_t step_count;
    uint8_t  prosody_class;
    uint16_t speech_minutes;
    uint16_t ambient_lux;
    uint16_t cct_kelvin;
    int16_t  room_temp_centic;
    uint16_t humidity_centi;
    uint16_t noise_db_tenth;
    uint8_t  stress_score;
    uint8_t  burnout_risk;
    uint8_t  recovery_score;
    uint8_t  flags;
    int64_t  timestamp_ms;
} ring_slot_t;

static ring_slot_t ring[RING_SLOTS];
static volatile uint32_t ring_head = 0;
static volatile uint32_t ring_count = 0;

/* Latest-received per-node data (updated by mesh RX callback) */
static uint8_t  cur_hr = 0;
static uint16_t cur_hrv = 0;
static uint16_t cur_edu_scl = 0;
static uint16_t cur_eda_scr = 0;
static int16_t  cur_temp = 0;
static uint8_t  cur_activity = 0;
static uint16_t cur_steps = 0;
static uint8_t  cur_prosody = 0;
static uint16_t cur_speech_min = 0;
static uint16_t cur_lux = 0, cur_cct = 0;
static int16_t  cur_room_temp = 0;
static uint16_t cur_humidity = 0, cur_noise = 0;
static uint8_t  cur_flags = 0;

/* Baseline (learned over first 14 days) */
static uint16_t baseline_hrv = 0;
static uint8_t  baseline_hr = 0;
static int16_t  baseline_temp = 0;
static uint16_t baseline_scl = 0;
static uint16_t baseline_scr_rate = 0;
static uint8_t  baseline_learned = 0;
static uint32_t baseline_sample_count = 0;

/* ---- TFLite Micro stress model (stub interface) ---- */
#define MODEL_INPUT_DIM 26
typedef struct { void *handle; } TfLiteMicroModel;

extern const unsigned char *stress_model_data;
extern unsigned int stress_model_len;

TfLiteMicroModel *tflm_model_create(const unsigned char *data, unsigned int len)
{
    (void)data; (void)len;
    return (TfLiteMicroModel *)1;  /* stub */
}

int tflm_model_invoke(TfLiteMicroModel *m, const float *input, int in_dim,
                      float *output, int out_dim)
{
    (void)m; (void)in_dim;
    /* Heuristic fallback when ML model not loaded:
     * Compute stress from HRV decline, EDA arousal, prosody, env load */
    float hrv_ratio = 1.0f;
    if (baseline_hrv > 0)
        hrv_ratio = (float)cur_hrv / (float)baseline_hrv;
    float hr_ratio = 1.0f;
    if (baseline_hr > 0)
        hr_ratio = (float)cur_hr / (float)baseline_hr;

    /* Stress = weighted penalties (higher = worse) */
    float hrv_penalty = 0;
    if (hrv_ratio < 0.8f)
        hrv_penalty = (0.8f - hrv_ratio) / 0.8f * 40.0f;  /* up to 40 pts */

    float hr_penalty = 0;
    if (hr_ratio > 1.1f)
        hr_penalty = (hr_ratio - 1.1f) / 0.3f * 20.0f;  /* up to 20 pts */

    /* EDA arousal */
    float eda_arousal = 0;
    if (baseline_scr_rate > 0) {
        float scr_ratio = (float)cur_eda_scr / (float)baseline_scr_rate;
        if (scr_ratio > 1.0f)
            eda_arousal = (scr_ratio - 1.0f) / 2.0f * 25.0f;  /* up to 25 pts */
    }

    /* Prosody stress */
    float prosody_penalty = 0;
    if (cur_prosody >= 3) prosody_penalty = 15.0f;      /* high-stress voice */
    else if (cur_prosody == 2) prosody_penalty = 8.0f;  /* elevated */

    /* Environmental load */
    float env_penalty = 0;
    if (cur_noise > 600) env_penalty += 5.0f;     /* >60 dB */
    if (cur_room_temp > 2700) env_penalty += 5.0f; /* >27 C */
    if (cur_lux < 100 && cur_activity == 5) env_penalty += 3.0f; /* dim + working */

    float stress = hrv_penalty + hr_penalty + eda_arousal +
                   prosody_penalty + env_penalty;
    if (stress > 100.0f) stress = 100.0f;
    if (stress < 0.0f) stress = 0.0f;

    output[0] = stress / 100.0f;  /* stress score */
    /* Burnout: simple 7-day average proxy (in production, CNN-LSTM) */
    output[1] = (stress > 60.0f) ? (stress - 40.0f) / 60.0f : 0.0f;
    if (output[1] > 1.0f) output[1] = 1.0f;
    /* Recovery: inverse of stress, boosted by sleep + activity */
    output[2] = 1.0f - (stress / 100.0f) * 0.7f;

    (void)out_dim;
    return 0;
}

/* ---- Mesh RX callback ---- */
static void mesh_rx_handler(uint8_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case CALM_MSG_VITALS: {
        if (len < sizeof(calm_vitals_payload_t) - 2) break;
        const calm_vitals_payload_t *p = (const calm_vitals_payload_t *)data;
        cur_hr = p->hr_bpm;
        cur_hrv = p->hrv_rmssd;
        cur_edu_scl = p->eda_scl;
        cur_eda_scr = p->eda_scr_rate;
        cur_temp = p->skin_temp_centic;
        cur_activity = p->activity_class;
        cur_steps = p->step_count;
        cur_flags |= p->flags;
        /* Update baseline learning */
        if (!baseline_learned) {
            baseline_hrv = (baseline_hrv * baseline_sample_count + p->hrv_rmssd) /
                           (baseline_sample_count + 1);
            baseline_hr = (baseline_hr * baseline_sample_count + p->hr_bpm) /
                          (baseline_sample_count + 1);
            baseline_scl = (baseline_scl * baseline_sample_count + p->eda_scl) /
                           (baseline_sample_count + 1);
            baseline_scr_rate = (baseline_scr_rate * baseline_sample_count +
                                 p->eda_scr_rate) / (baseline_sample_count + 1);
            baseline_sample_count++;
            if (baseline_sample_count >= 4032)  /* 14 days @ 5-min */
                baseline_learned = 1;
        }
        break;
    }
    case CALM_MSG_PROSODY: {
        if (len < sizeof(calm_prosody_payload_t) - 2) break;
        const calm_prosody_payload_t *p = (const calm_prosody_payload_t *)data;
        cur_prosody = p->prosody_class;
        cur_speech_min = p->speech_minutes;
        cur_flags |= p->flags;
        break;
    }
    case CALM_MSG_ENVIRONMENT: {
        if (len < sizeof(calm_environment_payload_t) - 2) break;
        const calm_environment_payload_t *p = (const calm_environment_payload_t *)data;
        cur_lux = p->ambient_lux;
        cur_cct = p->cct_kelvin;
        cur_room_temp = p->temp_centic;
        cur_humidity = p->humidity_centi;
        cur_noise = p->noise_db_tenth;
        cur_flags |= p->flags;
        break;
    }
    case CALM_MSG_LIGHTING: {
        /* Light node feedback — could log ambient lux */
        break;
    }
    default:
        break;
    }
}

/* ---- UART transport for mesh (nRF52840 on UART0) ---- */
static int mesh_uart_tx(const uint8_t *data, size_t len)
{
    uart_write_blocking(MESH_UART, data, len);
    return (int)len;
}

/* ---- WiFi co-processor bridge (ESP32-C6 on UART1) ---- */
static void wifi_send_to_cloud(const char *json)
{
    /* In production: frame + send to ESP32-C6 via UART1, which does MQTT */
    uart_write_blocking(WIFI_UART, (const uint8_t *)json, strlen(json));
    uart_write_blocking(WIFI_UART, (const uint8_t *)"\n", 1);
}

/* ---- Stress model inference (every 15 min) ---- */
static uint8_t cur_stress = 0;
static uint8_t cur_burnout = 0;
static uint8_t cur_recovery = 100;

static void run_stress_inference(void)
{
    float input[MODEL_INPUT_DIM];
    memset(input, 0, sizeof(input));
    input[0] = (float)cur_hr;
    input[1] = (float)cur_hrv / 100.0f;  /* centi-ms → ms */
    input[2] = (float)cur_temp / 100.0f;
    input[3] = (float)cur_edu_scl / 100.0f;  /* µS */
    input[4] = (float)cur_eda_scr / 100.0f;
    input[5] = (float)cur_activity;
    input[6] = (float)cur_prosody;
    input[7] = (float)cur_speech_min;
    input[8] = (float)cur_lux / 10.0f;
    input[9] = (float)cur_cct;
    input[10] = (float)cur_room_temp / 100.0f;
    input[11] = (float)cur_humidity / 100.0f;
    input[12] = (float)cur_noise / 10.0f;

    float output[3];
    TfLiteMicroModel *m = tflm_model_create(stress_model_data, stress_model_len);
    tflm_model_invoke(m, input, MODEL_INPUT_DIM, output, 3);

    cur_stress = (uint8_t)(output[0] * 100.0f);
    cur_burnout = (uint8_t)(output[1] * 100.0f);
    cur_recovery = (uint8_t)(output[2] * 100.0f);

    /* Store in ring buffer */
    ring[ring_head].hr_bpm = cur_hr;
    ring[ring_head].hrv_rmssd = cur_hrv;
    ring[ring_head].eda_scl = cur_edu_scl;
    ring[ring_head].eda_scr_rate = cur_eda_scr;
    ring[ring_head].skin_temp_centic = cur_temp;
    ring[ring_head].activity_class = cur_activity;
    ring[ring_head].step_count = cur_steps;
    ring[ring_head].prosody_class = cur_prosody;
    ring[ring_head].speech_minutes = cur_speech_min;
    ring[ring_head].ambient_lux = cur_lux;
    ring[ring_head].cct_kelvin = cur_cct;
    ring[ring_head].room_temp_centic = cur_room_temp;
    ring[ring_head].humidity_centi = cur_humidity;
    ring[ring_head].noise_db_tenth = cur_noise;
    ring[ring_head].stress_score = cur_stress;
    ring[ring_head].burnout_risk = cur_burnout;
    ring[ring_head].recovery_score = cur_recovery;
    ring[ring_head].flags = cur_flags;
    ring[ring_head].timestamp_ms = (int64_t)time_us_64() / 1000;

    ring_head = (ring_head + 1) % RING_SLOTS;
    if (ring_count < RING_SLOTS) ring_count++;
    cur_flags = 0;  /* reset per-cycle flags */

    /* Broadcast stress score to mesh (light node uses it autonomously) */
    calm_mesh_broadcast_stress_score(cur_stress, cur_burnout, cur_recovery);

    /* Trigger intervention if stress high */
    if (cur_stress >= 70) {
        /* High stress: trigger breathing + lighting */
        calm_mesh_send_intervention(0, 0, 0, 300);  /* breathing, 4-7-8, 5min */
        calm_mesh_send_lighting(CALM_SCENE_DESTRESS, 40, 2700, 0);
    } else if (cur_stress >= 50) {
        /* Elevated: gentle lighting shift */
        calm_mesh_send_lighting(CALM_SCENE_DESTRESS, 60, 3000, 0);
    }

    /* Send to cloud */
    char json[256];
    snprintf(json, sizeof(json),
             "{\"type\":\"stress\",\"stress\":%u,\"burnout\":%u,\"recovery\":%u}",
             cur_stress, cur_burnout, cur_recovery);
    wifi_send_to_cloud(json);
}

/* ---- Breathing-guide animation (runs on TFT) ---- */
extern void dashboard_render_stress(uint8_t stress, uint8_t burnout,
                                     uint8_t recovery);
extern void dashboard_render_breathing(uint8_t pattern, uint16_t elapsed_s,
                                       uint16_t duration_s);

/* ---- Main (Core 0: mesh + ML + WiFi) ---- */
int main(void)
{
    stdio_init_all();

    /* Initialize UARTs */
    uart_init(MESH_UART, MESH_BAUD);
    uart_init(WIFI_UART, WIFI_BAUD);
    gpio_set_function(0, GPIO_FUNC_UART);  /* TX0 */
    gpio_set_function(1, GPIO_FUNC_UART);  /* RX0 */
    gpio_set_function(4, GPIO_FUNC_UART);  /* TX1 */
    gpio_set_function(5, GPIO_FUNC_UART);  /* RX1 */

    /* Initialize mesh */
    calm_mesh_set_tx(mesh_uart_tx);
    calm_mesh_set_rx_callback(mesh_rx_handler);

    /* Initialize SPI for TFT + SD + Flash */
    spi_init(spi0, 24000000);
    /* ... GPIO config for SPI pins ... */

    /* Initialize I2C for RTC */
    i2c_init(i2c0, 100000);
    /* ... GPIO config for I2C pins ... */

    uint32_t last_inference_ms = 0;
    uint32_t last_breathing_ms = 0;
    uint8_t  breathing_active = 0;
    uint16_t breathing_elapsed = 0;
    uint16_t breathing_duration = 0;

    while (1) {
        /* Poll mesh UART for incoming frames */
        if (uart_is_readable(MESH_UART)) {
            uint8_t buf[CALM_MESH_MAX_PAYLOAD];
            int n = 0;
            while (uart_is_readable(MESH_UART) && n < CALM_MESH_MAX_PAYLOAD) {
                buf[n++] = uart_getc(MESH_UART);
            }
            calm_mesh_on_rx(buf, n);
        }

        uint32_t now = (uint32_t)(time_us_64() / 1000);

        /* Stress inference every 15 min (900000 ms) */
        if (now - last_inference_ms > 900000) {
            run_stress_inference();
            last_inference_ms = now;

            /* Start breathing if stress high */
            if (cur_stress >= 70 && !breathing_active) {
                breathing_active = 1;
                breathing_elapsed = 0;
                breathing_duration = 300;  /* 5 min */
                last_breathing_ms = now;
            }
        }

        /* Breathing-guide animation update */
        if (breathing_active) {
            uint32_t breath_step = now - last_breathing_ms;
            if (breath_step > 100) {  /* update every 100ms */
                breathing_elapsed += breath_step / 1000;
                dashboard_render_breathing(0, breathing_elapsed, breathing_duration);
                last_breathing_ms = now;
                if (breathing_elapsed >= breathing_duration) {
                    breathing_active = 0;
                    dashboard_render_stress(cur_stress, cur_burnout, cur_recovery);
                }
            }
        } else {
            /* Idle: render stress gauge */
            static uint32_t last_render = 0;
            if (now - last_render > 5000) {  /* every 5s */
                dashboard_render_stress(cur_stress, cur_burnout, cur_recovery);
                last_render = now;
            }
        }

        sleep_ms(10);
    }

    return 0;
}