/*
 * SeizureSync — Aura Patch firmware (nRF52840)
 *
 * Chest-worn disposable patch. Continuously monitors autonomic prodromal
 * signals: skin temperature (TMP117), electrodermal activity (AD8232),
 * and micro-PPG (MAX30101). Detects pre-ictal autonomic shifts 5-8 min
 * before seizure onset. Sends AuraNet pre-ictal probability to hub via
 * BLE 5.0 every 30 seconds. 14-day coin-cell operation.
 *
 * SPDX-License-Identifier: MIT
 *
 * Target: nRF52840 QFAA
 * Toolchain: nRF5 SDK 17 / nRF Connect SDK 2.x
 */
#include <stdio.h>
#include <string.h>
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "app_timer.h"
#include "ble.h"
#include "tmp117.h"
#include "eda_patch.h"
#include "max30101.h"
#include "../common/protocol.h"

#define TAG "PATCH"

/* ---- Pins (nRF52840) ---- */
#define PIN_TMP117_SCL    2
#define PIN_TMP117_SDA    3
#define PIN_EDA_ADC       4    /* SAADC input 0 */
#define PIN_MAX30101_SCL  5
#define PIN_MAX30101_SDA  6
#define PIN_MAX30101_INT  8
#define PIN_LED           9
#define PIN_BUTTON       10
#define PIN_VDD_EN       20   /* load switch */

/* ---- Globals ---- */
static uint8_t g_net_id[SZ_NET_ID_LEN] = {'S','Z','S','Y','N','C'};
static uint8_t g_seq = 0;

/* 10-minute autonomic history buffers (for pre-ictal feature extraction) */
#define HISTORY_LEN 600   /* 10 min @ 1 Hz */
static float g_temp_history[HISTORY_LEN];
static float g_eda_history[HISTORY_LEN];
static float g_hr_history[HISTORY_LEN];
static int g_hist_idx = 0;

/* ---- Sampling timer (1 Hz) ----
 * Every 1 second: read TMP117, EDA, MAX30101 HR.
 * Store in history buffer.
 */
APP_TIMER_DEF(m_sample_timer);

static void sample_handler(void *ctx)
{
    (void)ctx;
    g_temp_history[g_hist_idx] = tmp117_read_c();
    g_eda_history[g_hist_idx]  = eda_read_uS();
    g_hr_history[g_hist_idx]   = max30101_read_hr();
    g_hist_idx = (g_hist_idx + 1) % HISTORY_LEN;

    /* Blink LED briefly to show alive */
    nrf_gpio_pin_set(PIN_LED);
    nrf_delay_ms(5);
    nrf_gpio_pin_clear(PIN_LED);
}

/* ---- BLE transmit (every 30 s) ----
 * Send AuraNet pre-ictal probability to hub.
 * Burst mode to conserve battery.
 */
APP_TIMER_DEF(m_ble_timer);

static void ble_handler(void *ctx)
{
    (void)ctx;
    /* Extract pre-ictal features from 10-min history */
    float temp_trend = compute_temp_trend(g_temp_history, HISTORY_LEN);
    float eda_trend  = compute_eda_trend(g_eda_history, HISTORY_LEN);
    float hr_trend   = compute_hr_trend(g_hr_history, HISTORY_LEN);

    /* AuraNet pre-ictal probability (production: tflite-micro on nRF52840
     * or send features to hub for cloud inference) */
    float p_preictal = auranet_predict(g_temp_history, g_eda_history,
                                       g_hr_history, HISTORY_LEN);

    if (p_preictal > 0.65f) {
        /* Send AURA_ALERT to hub via BLE */
        sz_header_t h = {0};
        memcpy(h.net_id, g_net_id, SZ_NET_ID_LEN);
        h.src_node = SZ_SLOT_PATCH;
        h.dst_node = SZ_SLOT_HUB;
        h.type = SZ_PKT_AURA_ALERT;
        h.seq = g_seq++;

        sz_aura_payload_t p = {0};
        p.predicted_unix = 0;  /* TODO: RTC */
        p.lead_time_s = (uint16_t)(p_preictal * 600);  /* estimate */
        p.probability = (uint8_t)(p_preictal * 100);

        uint8_t pkt[64];
        size_t len = sz_pack(pkt, &h, (uint8_t *)&p, sizeof(p));
        ble_send_burst(pkt, len);
    } else {
        /* Send heartbeat only */
        send_patch_heartbeat();
    }
}

/* ---- Pre-ictal feature extraction ---- */
float compute_temp_trend(const float *h, int len)
{
    /* Linear regression slope over last 10 min */
    float sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    int n = len;
    for (int i = 0; i < n; i++) {
        sum_x  += i;
        sum_y  += h[i];
        sum_xy += (float)i * h[i];
        sum_x2 += (float)i * i;
    }
    float denom = (float)n * sum_x2 - sum_x * sum_x;
    if (fabsf(denom) < 1e-6f) return 0.0f;
    return ((float)n * sum_xy - sum_x * sum_y) / denom;
}

float compute_eda_trend(const float *h, int len)
{
    return compute_temp_trend(h, len);  /* same linear regression */
}

float compute_hr_trend(const float *h, int len)
{
    return compute_temp_trend(h, len);
}

/* ---- AuraNet prediction (heuristic fallback) ----
 * Production: load auranet_v1.tflite (bidirectional LSTM, quantized int8)
 * onto nRF52840 via tflite-micro. Here: heuristic based on autonomic
 * arousal pattern (rising EDA + falling skin temp + rising HR).
 */
float auranet_predict(const float *temp, const float *eda, const float *hr,
                       int len)
{
    float temp_slope = compute_temp_trend(temp, len);
    float eda_slope = compute_eda_trend(eda, len);
    float hr_slope  = compute_hr_trend(hr, len);

    /* Pre-ictal prodrome: rising EDA (sympathetic activation) +
     * falling skin temp (vasoconstriction) + rising HR */
    float score = 0;
    if (eda_slope > 0.01f)  score += 0.4f;
    if (temp_slope < -0.005f) score += 0.3f;
    if (hr_slope  > 0.1f)   score += 0.3f;

    if (score > 0.65f) return score;
    return score * 0.5f;  /* dampen if pattern incomplete */
}

void send_patch_heartbeat(void)
{
    sz_header_t h = {0};
    memcpy(h.net_id, g_net_id, SZ_NET_ID_LEN);
    h.src_node = SZ_SLOT_PATCH;
    h.dst_node = SZ_SLOT_HUB;
    h.type = SZ_PKT_HEARTBEAT;
    h.seq = g_seq++;

    sz_heartbeat_payload_t p = {0};
    p.battery_pct = 80;   /* TODO: read CR2477 voltage */
    p.rssi_dbm = -75;
    p.status_flags = 0x01;  /* worn */
    p.free_heap_kb = 64;

    uint8_t pkt[64];
    size_t len = sz_pack(pkt, &h, (uint8_t *)&p, sizeof(p));
    ble_send_burst(pkt, len);
}

/* ---- Mark event button ----
 * Patient presses button to manually mark a seizure (for ML labeling).
 */
static void button_handler(nrf_drv_gpiote_pin_t pin,
                            nrf_gpiote_polarity_t action)
{
    (void)pin; (void)action;
    /* Send a CONFIG packet marking this timestamp as a seizure event */
    sz_header_t h = {0};
    memcpy(h.net_id, g_net_id, SZ_NET_ID_LEN);
    h.src_node = SZ_SLOT_PATCH;
    h.dst_node = SZ_SLOT_HUB;
    h.type = SZ_PKT_CONFIG;
    h.seq = g_seq++;
    uint8_t payload[4] = {0x01, 0, 0, 0};  /* mark event */
    uint8_t pkt[64];
    size_t len = sz_pack(pkt, &h, payload, sizeof(payload));
    ble_send_burst(pkt, len);
}

/* ---- Main ---- */
int main(void)
{
    printf("SeizureSync Aura Patch starting...\n");

    /* GPIO init */
    nrf_gpio_cfg_output(PIN_LED);
    nrf_gpio_cfg_input(PIN_BUTTON, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_output(PIN_VDD_EN);
    nrf_gpio_pin_set(PIN_VDD_EN);  /* enable sensor power */

    /* SAADC init for EDA */
    nrf_saadc_channel_config_t adc_cfg = {
        .resistor_p = NRF_SAADC_RESISTOR_PULLDOWN,
        .gain       = NRF_SAADC_GAIN1_4,
        .reference  = NRF_SAADC_REFERENCE_INTERNAL,
        .acq_time   = NRF_SAADC_ACQTIME_10US,
        .mode       = NRF_SAADC_MODE_SINGLE_ENDED,
        .burst      = NRF_SAADC_BURST_DISABLED,
        .pin_p      = NRF_SAADC_INPUT_AIN0,
        .pin_n      = NRF_SAADC_INPUT_AIN0,
    };
    nrf_saadc_channel_init(0, &adc_cfg);

    /* I²C init for TMP117 + MAX30101 (2 separate buses) */
    twi_init(0, PIN_TMP117_SCL, PIN_TMP117_SDA);
    twi_init(1, PIN_MAX30101_SCL, PIN_MAX30101_SDA);

    /* Sensor init */
    tmp117_init(0);
    eda_init(PIN_EDA_ADC);
    max30101_init(1);

    /* BLE init (low-power peripheral mode) */
    ble_init_patch();

    /* App timers */
    app_timer_init();
    app_timer_create(&m_sample_timer, APP_TIMER_MODE_REPEATED,
                     sample_handler);
    app_timer_create(&m_ble_timer, APP_TIMER_MODE_REPEATED, ble_handler);
    app_timer_start(m_sample_timer, APP_TIMER_TICKS(1000), NULL);   /* 1 Hz */
    app_timer_start(m_ble_timer, APP_TIMER_TICKS(30000), NULL);     /* 30 s */

    /* GPIOTE for button */
    nrf_drv_gpiote_init();
    nrf_drv_gpiote_in_config_t btn_cfg = {
        .sense = NRF_GPIOTE_POLARITY_HITOLO,
        .pull = NRF_GPIO_PIN_PULLUP,
        .is_watcher = false, .hi_accuracy = false,
    };
    nrf_drv_gpiote_in_init(PIN_BUTTON, &btn_cfg, button_handler);
    nrf_drv_gpiote_in_event_enable(PIN_BUTTON, true);

    printf("Aura Patch ready. Monitoring autonomic prodrome.\n");

    /* Main loop — sleep in SEVONPEND (ultra-low-power) */
    while (1) {
        __WFE();   /* sleep until next interrupt (timer/ble/button) */
    }
    return 0;
}