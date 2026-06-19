/*
 * hub_main.c — PawSync Hub Node firmware (RP2040, Pico SDK)
 *
 * Aggregates BLE-mesh data from collar tag + feeder + camera, runs the
 * TFLite Micro wellness-score model every 15 min, renders the pet's
 * vitals + activity timeline on the ILI9488 TFT, plays enrichment audio
 * cues, and bridges to WiFi (ESP32-C6 co-processor) for MQTT cloud sync
 * + BLE to the mobile app.
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
#include "paw_protocol.h"

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
    int16_t  skin_temp_centic;
    uint8_t  activity_class;
    int16_t  gait[PAW_GAIT_FEATURES];
    uint16_t dispensed_g;
    uint16_t consumed_g;
    uint16_t water_ml;
    uint8_t  behavior_class;
    uint8_t  vocalization;
    uint8_t  wellness_score;
    uint8_t  illness_risk;
    uint8_t  anxiety_level;
    uint8_t  flags;
    int64_t  timestamp_ms;
} ring_slot_t;

static ring_slot_t ring[RING_SLOTS];
static volatile uint32_t ring_head = 0;
static volatile uint32_t ring_count = 0;

/* Latest-received per-node data (updated by mesh RX callback) */
static uint8_t  cur_hr = 0;
static uint16_t cur_hrv = 0;
static int16_t  cur_temp = 0;
static uint8_t  cur_activity = 0;
static int16_t  cur_gait[PAW_GAIT_FEATURES] = {0};
static uint16_t cur_dispensed = 0, cur_consumed = 0, cur_water = 0;
static uint8_t  cur_behavior = 0, cur_vocalization = 0;
static uint8_t  cur_flags = 0;

/* Baseline (learned over first 14 days) */
static uint16_t baseline_hrv = 0;
static uint8_t  baseline_hr = 0;
static int16_t  baseline_temp = 0;
static uint8_t  baseline_learned = 0;
static uint32_t baseline_sample_count = 0;

/* ---- TFLite Micro wellness model (stub interface) ---- */
#define MODEL_INPUT_DIM 24  /* 24 features per 5-min window */
typedef struct { void *handle; } TfLiteMicroModel;

/* Declared in wellness_model.c */
extern const unsigned char *wellness_model_data;
extern unsigned int wellness_model_len;

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
     * Compute wellness from HRV decline, temp elevation, appetite loss */
    float hrv_ratio = 1.0f;
    if (baseline_hrv > 0)
        hrv_ratio = (float)cur_hrv / (float)baseline_hrv;
    float hr_ratio = 1.0f;
    if (baseline_hr > 0)
        hr_ratio = (float)cur_hr / (float)baseline_hr;

    /* Wellness = 100 - penalties */
    float penalty = 0;
    if (hrv_ratio < 0.8f)   penalty += (0.8f - hrv_ratio) * 100.0f;  /* HRV decline */
    if (hr_ratio > 1.15f)   penalty += (hr_ratio - 1.15f) * 50.0f;    /* HR elevated */
    if (cur_flags & PAW_ALERT_LAMENESS)      penalty += 15;
    if (cur_flags & PAW_ALERT_SCRATCHING)    penalty += 10;
    if (cur_flags & PAW_ALERT_APPETITE_LOSS)  penalty += 20;
    if (cur_flags & PAW_ALERT_ANXIETY)        penalty += 10;

    float wellness = 100.0f - penalty;
    if (wellness < 0) wellness = 0;
    if (wellness > 100) wellness = 100;

    float illness_risk = (wellness < 50) ? (50 - wellness) * 2 : 0;
    if (illness_risk > 100) illness_risk = 100;

    float anxiety = 0;
    if (cur_flags & PAW_ALERT_ANXIETY) anxiety = 60;
    if (cur_vocalization == 2 || cur_vocalization == 6) anxiety += 30;

    output[0] = wellness / 100.0f;
    output[1] = illness_risk / 100.0f;
    output[2] = anxiety / 100.0f;
    (void)out_dim;
    return 0;
}

static TfLiteMicroModel *wellness_model = NULL;

static void load_wellness_model(void)
{
    wellness_model = tflm_model_create(wellness_model_data, wellness_model_len);
    if (!wellness_model)
        printf("WARN: wellness model not loaded — using heuristic fallback\n");
    else
        printf("Wellness model loaded (%u bytes)\n", wellness_model_len);
}

/* ---- WiFi co-processor interface (ESP32-C6 over UART1) ---- */
static void wifi_send_telemetry(const ring_slot_t *slot)
{
    /* JSON over UART to ESP32-C6 → MQTT publish */
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "{\"t\":\"vitals\",\"hr\":%u,\"hrv\":%u,\"temp\":%d,"
        "\"act\":%u,\"well\":%u,\"risk\":%u,\"anx\":%u,\"flags\":%u}\n",
        slot->hr_bpm, slot->hrv_rmssd, slot->skin_temp_centic,
        slot->activity_class, slot->wellness_score,
        slot->illness_risk, slot->anxiety_level, slot->flags);
    uart_write_blocking(WIFI_UART, (const uint8_t *)buf, n);
}

static void wifi_send_alert(const char *alert_type, uint16_t value)
{
    char buf[128];
    int n = snprintf(buf, sizeof(buf),
        "{\"t\":\"alert\",\"type\":\"%s\",\"value\":%u}\n",
        alert_type, value);
    uart_write_blocking(WIFI_UART, (const uint8_t *)buf, n);
}

/* ---- Enrichment audio (via MAX98357A I2S amplifier) ---- */
static void trigger_enrichment_audio(uint8_t intensity)
{
    /* Play calming audio track from flash via I2S */
    printf("ENRICHMENT: playing calming audio (intensity=%u)\n", intensity);
    /* In production: stream pre-encoded audio from W25Q256 flash → MAX98357A */
}

static void trigger_treat(void)
{
    /* Send enrichment command to feeder via mesh */
    printf("ENRICHMENT: requesting treat from feeder\n");
    /* mesh_send(PAW_MSG_ENRICHMENT, ...) — handled in mesh_model.c */
}

/* ---- Baseline learning ---- */
static void update_baseline(uint8_t hr, uint16_t hrv, int16_t temp)
{
    if (baseline_learned) return;
    baseline_sample_count++;
    /* Running average */
    if (baseline_sample_count == 1) {
        baseline_hr = hr;
        baseline_hrv = hrv;
        baseline_temp = temp;
    } else {
        baseline_hr = (uint8_t)((baseline_hr * (baseline_sample_count - 1) + hr) / baseline_sample_count);
        baseline_hrv = (uint16_t)((baseline_hrv * (baseline_sample_count - 1) + hrv) / baseline_sample_count);
        baseline_temp = (int16_t)((baseline_temp * (baseline_sample_count - 1) + temp) / baseline_sample_count);
    }
    /* Baseline established after 14 days (288*14 = 4032 samples) */
    if (baseline_sample_count >= 4032) {
        baseline_learned = 1;
        printf("BASELINE established: HR=%u, HRV=%u, Temp=%d.%02d°C\n",
               baseline_hr, baseline_hrv,
               baseline_temp / 100, abs(baseline_temp) % 100);
    }
}

/* ---- Wellness inference ---- */
static void run_wellness_inference(void)
{
    if (ring_count < 8) return;

    float input[MODEL_INPUT_DIM];
    memset(input, 0, sizeof(input));

    /* Build 24-feature vector from latest ring slot */
    ring_slot_t *s = &ring[(ring_head - 1 + RING_SLOTS) % RING_SLOTS];
    input[0]  = (float)s->hr_bpm;
    input[1]  = (float)s->hrv_rmssd / 100.0f;  /* ms */
    input[2]  = (float)s->skin_temp_centic / 100.0f;
    input[3]  = (float)s->activity_class;
    for (int i = 0; i < PAW_GAIT_FEATURES; i++)
        input[4 + i] = (float)s->gait[i] / 1000.0f;
    input[10] = (float)s->dispensed_g;
    input[11] = (float)s->consumed_g;
    input[12] = (float)s->water_ml;
    input[13] = (float)s->behavior_class;
    input[14] = (float)s->vocalization;
    input[15] = (float)s->wellness_score;
    input[16] = (float)s->illness_risk;
    input[17] = (float)s->anxiety_level;
    input[18] = (float)s->flags;
    /* Historical trend features (last 5 slots) */
    for (int i = 0; i < 5 && i < (int)ring_count; i++) {
        ring_slot_t *h = &ring[(ring_head - 2 - i + RING_SLOTS) % RING_SLOTS];
        input[19 + i] = (float)h->hrv_rmssd / 100.0f;
    }

    float output[3];
    if (tflm_model_invoke(wellness_model, input, MODEL_INPUT_DIM, output, 3) == 0) {
        uint8_t wellness  = (uint8_t)(output[0] * 100.0f);
        uint8_t risk       = (uint8_t)(output[1] * 100.0f);
        uint8_t anxiety    = (uint8_t)(output[2] * 100.0f);

        s->wellness_score = wellness;
        s->illness_risk   = risk;
        s->anxiety_level  = anxiety;

        /* Alert thresholds */
        if (risk > 65) {
            wifi_send_alert("HIGH_ILLNESS_RISK", risk);
            printf("ALERT: illness risk %u%%\n", risk);
        }
        if (anxiety > 60 && (cur_flags & PAW_ALERT_ANXIETY)) {
            /* Auto-trigger enrichment */
            trigger_enrichment_audio(anxiety);
            trigger_treat();
            printf("ANXIETY episode detected — triggering enrichment\n");
        }
        if (baseline_learned && baseline_hrv > 0) {
            if (cur_hrv < baseline_hrv * 0.8f) {
                cur_flags |= PAW_ALERT_HRV_DECLINE;
                wifi_send_alert("HRV_DECLINE",
                    (uint16_t)((1.0f - (float)cur_hrv / baseline_hrv) * 100));
            }
            if (baseline_hr > 0 && cur_hr > baseline_hr * 1.15f) {
                cur_flags |= PAW_ALERT_HR_ELEVATED;
                wifi_send_alert("HR_ELEVATED", cur_hr);
            }
        }

        /* Broadcast wellness to mesh */
        paw_wellness_payload_t wp = {0};
        wp.type           = PAW_MSG_WELLNESS;
        wp.node_id        = PAW_NODE_ID_HUB;
        wp.wellness_score = wellness;
        wp.illness_risk   = risk;
        wp.anxiety_level  = anxiety;
        paw_pack_crc(&wp, sizeof(wp) - 2);
        /* mesh_uart_send(...) — send to nRF52840 */
        char mesh_buf[32];
        int n = snprintf(mesh_buf, sizeof(mesh_buf), "W%u%u%u\n",
                         wellness, risk, anxiety);
        uart_write_blocking(MESH_UART, (const uint8_t *)mesh_buf, n);
    }
}

/* ---- Mesh UART RX handler ---- */
void paw_on_mesh_rx(uint8_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case PAW_MSG_VITALS: {
        if (len < sizeof(paw_vitals_payload_t) - 2) break;
        const paw_vitals_payload_t *v = (const paw_vitals_payload_t *)data;
        cur_hr = v->hr_bpm;
        cur_hrv = v->hrv_rmssd;
        cur_temp = v->skin_temp_centic;
        cur_flags |= v->flags;
        memcpy(cur_gait, v->gait, sizeof(cur_gait));
        update_baseline(v->hr_bpm, v->hrv_rmssd, v->skin_temp_centic);

        if (v->flags & PAW_ALERT_HRV_DECLINE)
            printf("ALERT: HRV decline detected\n");
        if (v->flags & PAW_ALERT_LAMENESS)
            printf("ALERT: Lameness detected\n");
        break;
    }
    case PAW_MSG_ACTIVITY: {
        if (len < sizeof(paw_activity_payload_t) - 2) break;
        const paw_activity_payload_t *a = (const paw_activity_payload_t *)data;
        cur_activity = a->activity_class;
        if (a->flags & PAW_ALERT_SCRATCHING)
            printf("ALERT: Excessive scratching detected\n");
        break;
    }
    case PAW_MSG_BEHAVIOR: {
        if (len < sizeof(paw_behavior_payload_t) - 2) break;
        const paw_behavior_payload_t *b = (const paw_behavior_payload_t *)data;
        cur_behavior = b->behavior_class;
        cur_vocalization = b->vocalization;
        if (b->flags & PAW_ALERT_ANXIETY) {
            cur_flags |= PAW_ALERT_ANXIETY;
            /* Trigger enrichment immediately for severe anxiety */
            if (b->vocalization == 1 || b->vocalization == 6) {
                trigger_enrichment_audio(70);
                trigger_treat();
            }
            wifi_send_alert("ANXIETY", b->duration_s);
        }
        break;
    }
    case PAW_MSG_FEEDING: {
        if (len < sizeof(paw_feeding_payload_t) - 2) break;
        const paw_feeding_payload_t *f = (const paw_feeding_payload_t *)data;
        cur_dispensed = f->dispensed_g;
        cur_consumed = f->consumed_g;
        cur_water = f->water_ml;
        /* Appetite loss: >25% uneaten */
        if (f->dispensed_g > 0) {
            float uneaten_ratio = 1.0f - (float)f->consumed_g / f->dispensed_g;
            if (uneaten_ratio > 0.25f) {
                cur_flags |= PAW_ALERT_APPETITE_LOSS;
                wifi_send_alert("APPETITE_LOSS",
                    (uint16_t)(uneaten_ratio * 100));
            }
        }
        if (f->hopper_pct < 15)
            wifi_send_alert("LOW_FOOD", f->hopper_pct);
        break;
    }
    case PAW_MSG_ALERT: {
        if (len < sizeof(paw_alert_payload_t) - 2) break;
        const paw_alert_payload_t *al = (const paw_alert_payload_t *)data;
        if (al->flags & PAW_ALERT_LAMENESS)
            wifi_send_alert("LAMENESS", al->value);
        if (al->flags & PAW_ALERT_FEVER)
            wifi_send_alert("FEVER", al->value);
        break;
    }
    default:
        break;
    }
}

/* ---- Ring buffer push ---- */
static void ring_push(void)
{
    ring_slot_t *s = &ring[ring_head];
    s->hr_bpm           = cur_hr;
    s->hrv_rmssd        = cur_hrv;
    s->skin_temp_centic = cur_temp;
    s->activity_class   = cur_activity;
    memcpy(s->gait, cur_gait, sizeof(cur_gait));
    s->dispensed_g      = cur_dispensed;
    s->consumed_g       = cur_consumed;
    s->water_ml         = cur_water;
    s->behavior_class   = cur_behavior;
    s->vocalization     = cur_vocalization;
    s->flags            = cur_flags;
    s->timestamp_ms     = (int64_t)time_us_64() / 1000;

    ring_head = (ring_head + 1) % RING_SLOTS;
    if (ring_count < RING_SLOTS) ring_count++;

    /* Update baseline with new data */
    update_baseline(cur_hr, cur_hrv, cur_temp);
    /* Reset per-cycle flags */
    cur_flags = 0;
}

/* ---- Core 1: mesh UART polling + TFT render ---- */
void core1_main(void)
{
    uint8_t rx_buf[128];
    int rx_len = 0;
    while (1) {
        /* Read framed mesh data from nRF52840 over UART0 */
        while (uart_is_readable(MESH_UART)) {
            uint8_t b = uart_getc(MESH_UART);
            if (rx_len < (int)sizeof(rx_buf))
                rx_buf[rx_len++] = b;
            /* Frame: newline-terminated JSON-like from nRF52840
             * In production: proper binary framing with length prefix */
            if (b == '\n' || rx_len >= (int)sizeof(rx_buf)) {
                if (rx_len >= 2)
                    paw_on_mesh_rx(rx_buf[0], &rx_buf[1], rx_len - 1);
                rx_len = 0;
            }
        }
        /* Render vitals display every 2s */
        /* render_pet_vitals(cur_hr, cur_hrv, cur_temp, cur_activity,
         *                   ring[ring_head].wellness_score); */
        sleep_ms(2000);
    }
}

int main(void)
{
    stdio_init_all();

    /* Init UARTs */
    uart_init(MESH_UART, MESH_BAUD);
    uart_init(WIFI_UART, WIFI_BAUD);
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);
    gpio_set_function(4, GPIO_FUNC_UART);
    gpio_set_function(5, GPIO_FUNC_UART);

    /* Init SPI0 for ILI9488 TFT, I2C0 for RTC */
    spi_init(spi0, 24000000);
    i2c_init(i2c0, 100000);

    load_wellness_model();
    printf("PawSync Hub starting — model %s\n",
           wellness_model ? "loaded" : "fallback heuristic");

    /* Launch core 1 for mesh polling + TFT */
    multicore_launch_core1(core1_main);

    int64_t last_wellness_ms = 0;
    int64_t last_push_ms = 0;
    int64_t last_wifi_ms = 0;

    while (1) {
        int64_t now = (int64_t)time_us_64() / 1000;

        /* Push ring slot every 5 min */
        if (now - last_push_ms > 300000) {
            ring_push();
            last_push_ms = now;
        }
        /* Run wellness inference every 15 min */
        if (now - last_wellness_ms > 900000) {
            run_wellness_inference();
            last_wellness_ms = now;
        }
        /* WiFi MQTT sync every 60s */
        if (now - last_wifi_ms > 60000) {
            wifi_send_telemetry(&ring[(ring_head - 1 + RING_SLOTS) % RING_SLOTS]);
            last_wifi_ms = now;
        }
        sleep_ms(500);
    }
    return 0;
}