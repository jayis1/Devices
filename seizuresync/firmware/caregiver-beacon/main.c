/*
 * SeizureSync — Caregiver Beacon firmware (ESP32-C3)
 *
 * Portable alert device for the caregiver. Receives seizure/aura/SUDEP
 * alerts via Sub-GHz mesh. Emits haptic (DRV2605L), audio (MAX98357A),
 * and visual (WS2812 8x8 RGB + 2.9" e-ink) alerts. 7-day battery.
 * Has Acknowledge, Dispatch 911, and Test buttons.
 * Can relay mesh traffic (patch→hub if patch out of direct range).
 *
 * SPDX-License-Identifier: MIT
 *
 * Target: ESP32-C3-MINI-1-N4
 * Toolchain: ESP-IDF v5.1
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

#include "../common/protocol.h"
#include "../common/tdma.h"
#include "sx1262.h"
#include "alert_driver.h"

static const char *TAG = "BEACON";

/* ---- Pins (ESP32-C3) ---- */
#define PIN_SX1262_NSS   0
#define PIN_SX1262_SCK   1
#define PIN_SX1262_MISO  2
#define PIN_SX1262_MOSI  3
#define PIN_SX1262_DIO0  4
#define PIN_SX1262_RST   5
#define PIN_SX1262_BUSY  6
#define PIN_BTN_ACK      21
#define PIN_BTN_911      22
#define PIN_BTN_TEST     23

/* ---- Globals ---- */
static uint8_t g_net_id[SZ_NET_ID_LEN] = {'S','Z','S','Y','N','C'};
static uint8_t g_seq = 0;

/* ---- Mesh listener ----
 * Beacon listens continuously (it's not slot-synchronized; it's a
 * portable receiver). Uses CAD (channel activity detection) mode
 * to conserve battery, wakes on packet detection.
 */
static void mesh_listen_task(void *arg)
{
    (void)arg;
    while (1) {
        uint8_t rx_buf[SZ_RADIO_MAX_PKT];
        int rlen = sx1262_receive(rx_buf, sizeof(rx_buf), 2000);  /* 2s CAD */
        if (rlen > 0) {
            sz_header_t h;
            uint8_t payload[128];
            size_t plen;
            if (sz_parse(rx_buf, rlen, &h, payload, &plen) == 0) {
                handle_alert(&h, payload, plen);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ---- Handle incoming alert ---- */
void handle_alert(const sz_header_t *h, const uint8_t *payload, size_t plen)
{
    (void)plen;
    switch (h->type) {
    case SZ_PKT_SEIZURE_ALERT: {
        sz_seizure_payload_t *s = (sz_seizure_payload_t *)payload;
        ESP_LOGW(TAG, "🚨 SEIZURE ALERT: semiology=%d severity=%d conf=%d%%",
                 s->semiology, s->severity, s->confidence);
        alert_seizure(s);
        break;
    }
    case SZ_PKT_AURA_ALERT: {
        sz_aura_payload_t *a = (sz_aura_payload_t *)payload;
        ESP_LOGW(TAG, "⚠️ AURA: lead=%ds prob=%d%%", a->lead_time_s,
                 a->probability);
        alert_aura(a);
        break;
    }
    case SZ_PKT_SUDEP_ALERT: {
        sz_sudep_payload_t *s = (sz_sudep_payload_t *)payload;
        ESP_LOGW(TAG, "🆘 SUDEP: apnea=%ds spo2=%d%% prone=%d",
                 s->apnea_duration_s, s->spo2_pct, s->prone_flag);
        alert_sudep(s);
        break;
    }
    case SZ_PKT_ACK: {
        /* Hub forwarding caregiver's own ack — ignore */
        break;
    }
    default:
        ESP_LOGI(TAG, "Pkt 0x%02x from %d", h->type, h->src_node);
    }
}

/* ---- Alert driver callbacks (alert_driver.c) ---- */
void alert_seizure(const sz_seizure_payload_t *s)
{
    /* 1. E-ink display: patient, seizure type, onset, status */
    display_show_seizure(s);
    /* 2. RGB matrix: solid red */
    rgb_set_color(255, 0, 0);
    /* 3. Haptic: 3-burst pattern (seizure-specific) */
    haptic_pattern_seizure();
    /* 4. Audio: 85 dB alarm + voice prompt */
    audio_play_seizure_alarm();
    /* 5. Start ack timeout — if no ack in 90s, escalate */
    start_ack_timeout(s->onset_unix, 90);
}

void alert_aura(const sz_aura_payload_t *a)
{
    display_show_aura(a);
    rgb_set_color(255, 200, 0);   /* yellow */
    haptic_pattern_aura();
    audio_play_aura_warning();
}

void alert_sudep(const sz_sudep_payload_t *s)
{
    display_show_sudep(s);
    rgb_set_color(255, 0, 0);
    rgb_set_blink(true);
    haptic_pattern_sudep();
    audio_play_sudep_alarm();   /* loudest, most urgent */
    /* SUDEP: immediate escalation, no ack timeout — auto-dispatch 911
     * after 60s if caregiver doesn't respond */
    start_ack_timeout(0, 60);
}

/* ---- Button handlers ---- */
void IRAM_ATTR btn_ack_isr(void *arg)
{
    (void)arg;
    /* Send ACK to hub */
    sz_header_t h = {0};
    memcpy(h.net_id, g_net_id, SZ_NET_ID_LEN);
    h.src_node = SZ_SLOT_BEACON;
    h.dst_node = SZ_SLOT_HUB;
    h.type = SZ_PKT_ACK;
    h.seq = g_seq++;
    sz_ack_payload_t p = {0};
    p.event_unix = 0;   /* TODO: from last event */
    p.action = 0;       /* ack */
    uint8_t pkt[64];
    size_t len = sz_pack(pkt, &h, (uint8_t *)&p, sizeof(p));
    sx1262_send(pkt, len);

    /* Stop alarm */
    rgb_set_color(0, 0, 0);
    audio_stop();
    haptic_stop();
    display_show_idle();
}

void IRAM_ATTR btn_911_isr(void *arg)
{
    (void)arg;
    /* Send DISPATCH to hub → hub triggers Twilio 911 call */
    sz_header_t h = {0};
    memcpy(h.net_id, g_net_id, SZ_NET_ID_LEN);
    h.src_node = SZ_SLOT_BEACON;
    h.dst_node = SZ_SLOT_HUB;
    h.type = SZ_PKT_DISPATCH;
    h.seq = g_seq++;
    sz_ack_payload_t p = {0};
    p.action = 1;   /* dispatch 911 */
    uint8_t pkt[64];
    size_t len = sz_pack(pkt, &h, (uint8_t *)&p, sizeof(p));
    sx1262_send(pkt, len);

    /* Visual feedback: solid blue LED */
    rgb_set_color(0, 0, 255);
    audio_play_dispatched();
}

void IRAM_ATTR btn_test_isr(void *arg)
{
    (void)arg;
    /* Self-test: play each alert type briefly */
    haptic_pattern_seizure();
    vTaskDelay(pdMS_TO_TICKS(500));
    audio_play_test();
    rgb_set_color(0, 255, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    rgb_set_color(0, 0, 0);
}

/* ---- Ack timeout → escalation ---- */
void start_ack_timeout(uint32_t event_unix, int timeout_s)
{
    /* Production: start a FreeRTOS timer. On expiry, auto-escalate
     * to family/911 via hub. Here: log only. */
    ESP_LOGW(TAG, "Ack timeout in %ds for event %u", timeout_s, event_unix);
    /* TODO: if no ack within timeout_s, send DISPATCH to hub */
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "SeizureSync Caregiver Beacon starting...");

    /* GPIO init for buttons */
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL<<PIN_BTN_ACK) | (1ULL<<PIN_BTN_911)
                      | (1ULL<<PIN_BTN_TEST),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&btn_conf);
    gpio_isr_handler_add(PIN_BTN_ACK, btn_ack_isr, NULL);
    gpio_isr_handler_add(PIN_BTN_911, btn_911_isr, NULL);
    gpio_isr_handler_add(PIN_BTN_TEST, btn_test_isr, NULL);

    /* I²C for DRV2605L haptic + e-ink */
    i2c_config_t i2c = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 20, .scl_io_num = 19,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &i2c);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    /* SPI for SX1262 + e-ink */
    spi_bus_config_t buscfg = {
        .mosi_io_num = 3, .miso_io_num = 2, .sclk_io_num = 1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, 0);

    /* SX1262 Sub-GHz radio */
    sx1262_init(PIN_SX1262_NSS, PIN_SX1262_RST, PIN_SX1262_DIO0,
                PIN_SX1262_BUSY, PIN_SX1262_SCK, PIN_SX1262_MISO,
                PIN_SX1262_MOSI);
    sx1262_set_frequency(SZ_RADIO_FREQ_HZ);
    sx1262_set_tx_power(SZ_RADIO_TX_DBM);
    sx1262_set_modem_params(SZ_RADIO_BW_HZ, SZ_RADIO_SF, SZ_RADIO_CR);

    /* Alert drivers */
    alert_driver_init();

    /* Display idle screen */
    display_show_idle();

    /* Start mesh listener */
    xTaskCreate(mesh_listen_task, "mesh", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Beacon ready. Listening for alerts.");
}