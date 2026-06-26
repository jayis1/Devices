/*
 * OralSync Toothbrush Node — main firmware (nRF52840, bare-metal)
 *
 * Streams IMU (6-DoF) + pressure (FSR) at 50 Hz over BLE OSMP to the Hub.
 * LRA haptics for quadrant pacing + over-pressure warning.
 * Runs ~30 days on 500 mAh Li-Po (active only ~4 min/day).
 *
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include "osmp.h"
#include "osmp_sensors.h"

#define SAMPLE_HZ        50u
#define SAMPLE_PERIOD_MS (1000u / SAMPLE_HZ)
#define SESSION_TIMEOUT_S 180u   /* auto-end after 3 min */

static uint8_t  s_seq = 0;
static uint8_t  s_tx[OSMT_MAX_FRAME];
static uint8_t  s_rx[OSMT_MAX_FRAME];
static volatile int s_session_active = 0;
static volatile uint32_t s_session_start_s = 0;
static uint32_t s_session_id = 0;
static uint8_t  s_user_id = 0;
static uint8_t  s_coverage[8] = {0};

/* BLE OSMP transport — provided by ble_layer.c (nRF SDK softdevice) */
extern void ble_osmp_init(void);
extern void ble_osmp_send(const uint8_t *frame, size_t len);
extern int  ble_osmp_recv(uint8_t *out, size_t cap, uint32_t timeout_ms);
extern void ble_osmp_advertise_start(void);
extern void ble_osmp_advertise_stop(void);

static void send_frame(size_t len) { ble_osmp_send(s_tx, len); }

static void handle_coach_cue(const uint8_t *p, uint8_t n)
{
    if (n < 2) return;
    uint8_t cue = p[0], arg = p[1];
    switch (cue) {
        case 0x01: haptics_pulse(0); break;        /* short: advance */
        case 0x02: haptics_pulse(1); break;        /* long: over-pressure */
        case 0x03: haptics_pulse(2); break;        /* double: missed surface */
        case 0x10: {                                 /* QUAD_PACE */
            /* arg = quadrant 1..4 */
            haptics_pulse(0);
            break;
        }
        default: break;
    }
}

static void decode_coverage_from_imu(const imu_sample_t *s)
{
    /* Crude on-node sextant estimate from accelerometer orientation.
     * The Hub does the high-fidelity mapping; this populates a rough
     * coverage bitmap for offline / disconnected sessions. */
    int sextant;
    if      (s->az >  500) sextant = 0;   /* upper buccal */
    else if (s->az < -500) sextant = 1;   /* lower buccal */
    else if (s->ay >  500) sextant = 2;   /* upper lingual */
    else if (s->ay < -500) sextant = 3;   /* lower lingual */
    else if (s->ax >  500) sextant = 4;   /* occlusal R */
    else if (s->ax < -500) sextant = 5;   /* occlusal L */
    else return;
    s_coverage[sextant / 8] |= (1u << (sextant % 8));
}

static void begin_session(uint8_t user_id)
{
    s_session_active = 1;
    s_session_start_s = rtc_now_unix();
    s_session_id = rtc_now_unix();  /* ephemeral id */
    s_user_id = user_id;
    memset(s_coverage, 0, sizeof(s_coverage));
    ble_osmp_advertise_start();      /* connect to hub */

    /* SESSION_START frame */
    uint8_t p[9];
    memcpy(&p[0], &s_session_id, 4);
    p[4] = user_id;
    uint32_t start = s_session_start_s;
    memcpy(&p[5], &start, 4);
    size_t n = osmp_encode(s_tx, sizeof(s_tx), OSMP_SESSION_START, s_seq++, p, sizeof(p));
    send_frame(n);
}

static void end_session(void)
{
    if (!s_session_active) return;
    s_session_active = 0;
    uint16_t dur = (uint16_t)(rtc_now_unix() - s_session_start_s);
    size_t n = osmp_build_session_end(s_tx, sizeof(s_tx), s_seq++,
                                      s_session_id, dur, s_coverage);
    send_frame(n);
    ble_osmp_advertise_stop();
}

int main(void)
{
    imu_init();
    pressure_init();
    haptics_init();
    ble_osmp_init();

    /* Send HELLO once paired (will be triggered by connection event in real code) */
    size_t n = osmp_build_hello(s_tx, sizeof(s_tx), OSMP_NODE_TOOTHBRUSH,
                                /*hw_rev*/2, /*fw*/0x0104, /*caps*/0x0003);
    send_frame(n);

    /* Button press starts session — simulate with periodic check */
    imu_sample_t imu;
    uint32_t last_sample_ms = 0;
    uint32_t last_timeout_check_s = 0;

    while (1) {
        uint32_t now_s = rtc_now_unix();

        /* Poll for hub commands (non-blocking, short timeout) */
        int rlen = ble_osmp_recv(s_rx, sizeof(s_rx), 5 /*ms*/);
        if (rlen > 0) {
            uint8_t type, seq, plen;
            uint8_t payload[OSMP_MAX_PAYLOAD];
            if (osmp_decode(s_rx, (size_t)rlen, &type, &seq, payload, &plen)) {
                switch (type) {
                    case OSMP_COACH_CUE:  handle_coach_cue(payload, plen); break;
                    case OSMP_QUAD_PACE:  haptics_pulse(0); break;
                    case OSMP_OTA_CHUNK:  /* forward to bootloader */      break;
                    case OSMP_PING: {
                        uint8_t pong[4];
                        uint32_t t = now_s;
                        memcpy(pong, &t, 4);
                        size_t o = osmp_encode(s_tx, sizeof(s_tx), OSMP_PONG, seq, pong, 4);
                        send_frame(o);
                        break;
                    }
                    default: break;
                }
            }
        }

        /* Session sampling loop at 50 Hz */
        if (s_session_active) {
            uint32_t now_ms = last_sample_ms + SAMPLE_PERIOD_MS; /* placeholder for sys tick */
            imu_read(&imu);
            uint16_t p_cN = pressure_read_cN();

            /* Over-pressure warning (>3.5 N) */
            if (p_cN > 350) haptics_pulse(1);

            size_t o = osmp_build_imu(s_tx, sizeof(s_tx), s_seq++,
                                      imu.ax, imu.ay, imu.az,
                                      imu.gx, imu.gy, imu.gz, (uint16_t)now_ms);
            send_frame(o);

            o = osmp_build_pressure(s_tx, sizeof(s_tx), s_seq++, p_cN, (uint16_t)now_ms);
            send_frame(o);

            decode_coverage_from_imu(&imu);

            /* Auto-end after timeout */
            if ((now_s - last_timeout_check_s) >= 1u) {
                last_timeout_check_s = now_s;
                if ((now_s - s_session_start_s) >= SESSION_TIMEOUT_S)
                    end_session();
            }
        }

        /* Deep sleep between samples (WFE); IMU INT1 wakes on motion.
         * In real firmware: sd_app_evt_wait() here. */
        __asm__ volatile ("wfe");
    }
    return 0;
}

/* Stub: button ISR triggers session start/stop.
 * Wired to nRF52840 GPIO edge interrupt on the brush power button. */
void on_button_press(void)
{
    if (!s_session_active) begin_session(s_user_id ? s_user_id : 1u);
    else                   end_session();
}