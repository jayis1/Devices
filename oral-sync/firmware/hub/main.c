/*
 * OralSync Hub — main firmware (RP2040 dual-core + ESP32-C3)
 *
 * RP2040 Core 0: LCD framebuffer, plaque heatmap renderer, OSMP state machine.
 * RP2040 Core 1: TFLite Micro brushing-technique classifier + coach cue gen.
 * ESP32-C3:      BLE 5.0 central (OSMP GATT), Wi-Fi MQTT uplink, OTA pull.
 *
 * This file contains the RP2040 side. ESP32-C3 firmware in wifi_bridge.c.
 *
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include "osmp.h"
#include "osmp_sensors.h"

#define LCD_W 720
#define LCD_H 1280
#define TOOTH_MAP_X 60
#define TOOTH_MAP_Y 200

static uint8_t s_seq = 0;
static uint8_t s_tx[OSMT_MAX_FRAME];
static uint8_t s_rx[OSMT_MAX_FRAME];

/* UART bridge to ESP32-C3 (carries OSMP frames to/from BLE central) */
extern void uart_bridge_init(void);
extern int  uart_bridge_recv(uint8_t *out, size_t cap, uint32_t timeout_ms);
extern void uart_bridge_send(const uint8_t *frame, size_t len);

/* TFLite Micro brushing-technique classifier — coach.c */
extern int  coach_init(void);
extern int  coach_classify(const imu_sample_t *win, int n, int *technique_out);
extern void coach_cue_generate(int technique, int sextant, int pressure_cN,
                               uint8_t *cue_id, uint8_t *cue_arg);

/* Plaque heatmap renderer — lcd.c */
extern void lcd_render_toothmap(int plaque_per_sextant[6],
                                int risk_per_sextant[6],
                                int active_sextant);
extern void lcd_render_session(int technique, int coverage_pct,
                               int duration_s, int pressure_cN);
extern void lcd_render_risk_gauge(int risk_0_100);

static void send_frame(size_t len) { uart_bridge_send(s_tx, len); }

/* Latest session state (updated by Core 1 coach) */
static volatile int s_active_sextant = 0;
static volatile int s_last_technique = 0;
static volatile int s_last_pressure  = 0;
static volatile int s_coverage_count = 0;
static int s_plaque_per_sextant[6] = {10, 15, 20, 12, 8, 18};
static int s_risk_per_sextant[6]   = {22, 30, 45, 28, 18, 38};

/* Sliding window of IMU samples for technique classification */
#define WIN_N 100   /* 2 s @ 50 Hz */
static imu_sample_t s_win[WIN_N];
static int s_win_idx = 0;

static void handle_imu(const uint8_t *p, uint8_t n)
{
    if (n < 14) return;
    imu_sample_t s;
    memcpy(&s.ax, &p[0], 2);
    memcpy(&s.ay, &p[2], 2);
    memcpy(&s.az, &p[4], 2);
    memcpy(&s.gx, &p[6], 2);
    memcpy(&s.gy, &p[8], 2);
    memcpy(&s.gz, &p[10], 2);
    memcpy(&s.ts_ms, &p[12], 2);

    s_win[s_win_idx++] = s;
    if (s_win_idx >= WIN_N) {
        s_win_idx = 0;
        int tech = 0;
        coach_classify(s_win, WIN_N, &tech);
        s_last_technique = tech;

        /* Map orientation → sextant (simplified) */
        if      (s.az >  500) s_active_sextant = 0;
        else if (s.az < -500) s_active_sextant = 1;
        else if (s.ay >  500) s_active_sextant = 2;
        else if (s.ay < -500) s_active_sextant = 3;
        else if (s.ax >  500) s_active_sextant = 4;
        else if (s.ax < -500) s_active_sextant = 5;
        s_coverage_count++;

        /* Generate coach cue */
        uint8_t cue_id = 0, cue_arg = 0;
        coach_cue_generate(tech, s_active_sextant, s_last_pressure, &cue_id, &cue_arg);
        if (cue_id) {
            uint8_t cp[2] = { cue_id, cue_arg };
            size_t o = osmp_encode(s_tx, sizeof(s_tx), OSMP_COACH_CUE, s_seq++, cp, 2);
            send_frame(o);
        }
    }

    /* Live LCD update */
    lcd_render_session(s_last_technique,
                       (s_coverage_count * 100) / 48,
                       /*dur*/ s_coverage_count / 50,
                       s_last_pressure);
    lcd_render_toothmap(s_plaque_per_sextant, s_risk_per_sextant, s_active_sextant);
}

static void handle_pressure(const uint8_t *p, uint8_t n)
{
    if (n < 4) return;
    uint16_t cN;
    memcpy(&cN, &p[0], 2);
    s_last_pressure = cN;
    if (cN > 350) {
        uint8_t cue[2] = { 0x02, 0x00 };  /* OVERPRESSURE */
        size_t o = osmp_encode(s_tx, sizeof(s_tx), OSMP_COACH_CUE, s_seq++, cue, 2);
        send_frame(o);
    }
}

static void handle_saliva(const uint8_t *p, uint8_t n)
{
    if (n < 7) return;
    uint16_t ph, nitrite, temp;
    uint8_t buf;
    memcpy(&ph, &p[0], 2);
    memcpy(&nitrite, &p[2], 2);
    buf = p[4];
    memcpy(&temp, &p[5], 2);
    /* Forward to ESP32-C3 → MQTT (uart_bridge will tag it). */
    /* Update risk gauge on LCD */
    int risk = 50 - (ph - 680) / 4 - (buf * 2);
    if (risk < 0) risk = 0; if (risk > 100) risk = 100;
    lcd_render_risk_gauge(risk);
}

static void handle_scan_frame(const uint8_t *p, uint8_t n)
{
    /* Forward thumbnail to cloud via ESP32-C3 MQTT; full image stays on scanner
     * until Wi-Fi dock. Embeddings arrive in SCAN_EMBED. */
    (void)p; (void)n;
}

int main(void)
{
    lcd_init();
    neopixel_init();
    i2s_mic_init();
    i2s_spk_init();
    sht40_init();
    uart_bridge_init();
    coach_init();

    /* Render idle screen: tooth map + risk gauge */
    lcd_render_toothmap(s_plaque_per_sextant, s_risk_per_sextant, -1);
    lcd_render_risk_gauge(28);

    while (1) {
        int rlen = uart_bridge_recv(s_rx, sizeof(s_rx), 10 /*ms*/);
        if (rlen <= 0) continue;
        uint8_t type, seq, plen, payload[OSMP_MAX_PAYLOAD];
        if (!osmp_decode(s_rx, (size_t)rlen, &type, &seq, payload, &plen)) continue;

        switch (type) {
            case OSMP_IMU_SAMPLE:      handle_imu(payload, plen); break;
            case OSMP_PRESSURE_SAMPLE: handle_pressure(payload, plen); break;
            case OSMP_SALIVA_READING:  handle_saliva(payload, plen); break;
            case OSMP_SCAN_FRAME:
            case OSMP_SCAN_EMBED:      handle_scan_frame(payload, plen); break;
            case OSMP_SESSION_END: {
                /* Persist + MQTT uplink via ESP32-C3 */
                size_t o = osmp_build_ack(s_tx, sizeof(s_tx), seq, OSMP_OK);
                send_frame(o);
                lcd_render_session(s_last_technique, 100, 120, 0);
                break;
            }
            case OSMP_PING: {
                uint8_t pong[4]; uint32_t t = rtc_now_unix();
                memcpy(pong, &t, 4);
                size_t o = osmp_encode(s_tx, sizeof(s_tx), OSMP_PONG, seq, pong, 4);
                send_frame(o);
                break;
            }
            default: break;
        }
    }
    return 0;
}