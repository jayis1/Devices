/*
 * OralSync Plaque Scanner — main firmware (ESP32-S3, ESP-IDF bare-metal style)
 *
 * Multispectral intraoral imaging: 405/470/525/660/850 nm LED ring + OV5640.
 * On-device TFLite Micro U-Net-tiny plaque segmentation + gingivitis preview.
 * Streams scan frame thumbnails + embeddings over BLE OSMP to Hub.
 *
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include "osmp.h"
#include "osmp_sensors.h"

#define SCAN_BANDS    6     /* white,405,470,525,660,850 */
#define THUMB_W       80
#define THUMB_H       60
#define EMBED_DIM     32    /* floats */

static uint8_t s_seq = 0;
static uint8_t s_tx[OSMT_MAX_FRAME];
static uint8_t s_rx[OSMT_MAX_FRAME];
static uint32_t s_scan_id = 0;

extern void ble_osmp_init(void);
extern void ble_osmp_send(const uint8_t *frame, size_t len);
extern int  ble_osmp_recv(uint8_t *out, size_t cap, uint32_t timeout_ms);
extern void wifi_ota_check(void);

/* TFLite Micro model handles — provided by tflite_plaque.c */
extern int  plaque_seg_init(void);
extern int  plaque_seg_run(const uint8_t *rgb565, int w, int h,
                           uint8_t *mask_out, float *plaque_pct);
extern int  gingivitis_classify(const uint8_t *rgb565, int w, int h, int *severity);
extern int  lesion_detect(const uint8_t *rgb565, int w, int h, int *n_lesions);
extern void feature_embedding(const uint8_t *rgb565, int w, int h, float *emb_out);

static void send_frame(size_t len) { ble_osmp_send(s_tx, len); }

static void run_scan(void)
{
    s_scan_id = rtc_now_unix();
    uint8_t frame[THUMB_W * THUMB_H * 2];   /* RGB565 thumbnail buffer */
    uint8_t mask[THUMB_W * THUMB_H];
    float   emb[EMBED_DIM];

    plaque_seg_init();

    for (int band = 0; band < SCAN_BANDS; band++) {
        spectral_set_band(band);
        camera_set_band(band);
        size_t cap_len = 0;
        if (camera_capture(frame, sizeof(frame), &cap_len, band) != 0) continue;

        /* On-device plaque segmentation (405 nm band is most informative) */
        float plaque_pct = 0.0f;
        if (band == 1) {  /* 405 nm */
            plaque_seg_run(frame, THUMB_W, THUMB_H, mask, &plaque_pct);
            gingivitis_classify(frame, THUMB_W, THUMB_H, NULL);
            lesion_detect(frame, THUMB_W, THUMB_H, NULL);
            feature_embedding(frame, THUMB_W, THUMB_H, emb);
        }

        /* Stream JPEG thumbnail to Hub */
        /* (Real impl: JPEG-encode `frame` into ≤160 B; here send raw marker) */
        uint8_t p[OSMP_MAX_PAYLOAD];
        p[0] = (s_scan_id      ) & 0xFF;
        p[1] = (s_scan_id >>  8) & 0xFF;
        p[2] = (s_scan_id >> 16) & 0xFF;
        p[3] = (s_scan_id >> 24) & 0xFF;
        p[4] = (uint8_t)band;
        p[5] = THUMB_W;
        p[6] = THUMB_H;
        size_t thumb_len = cap_len < 160 ? cap_len : 160;
        memcpy(&p[7], frame, thumb_len);
        size_t n = osmp_encode(s_tx, sizeof(s_tx), OSMP_SCAN_FRAME, s_seq++,
                               p, (uint8_t)(7u + thumb_len));
        send_frame(n);
    }

    /* Send feature embedding for cloud longitudinal lesion tracking */
    uint8_t p[4 + EMBED_DIM * 4];
    memcpy(&p[0], &s_scan_id, 4);
    memcpy(&p[4], emb, EMBED_DIM * 4);
    size_t n = osmp_encode(s_tx, sizeof(s_tx), OSMP_SCAN_EMBED, s_seq++,
                           p, (uint8_t)sizeof(p));
    send_frame(n);

    spectral_off();
    buzzer_beep(120);  /* "scan complete" cue */
}

int main(void)
{
    camera_init();
    vl53l1x_init();
    sht40_init();
    spectral_off();
    buzzer_init();
    ble_osmp_init();
    plaque_seg_init();

    /* HELLO */
    size_t n = osmp_build_hello(s_tx, sizeof(s_tx), OSMP_NODE_SCANNER,
                                /*hw*/1, /*fw*/0x0102, /*caps*/0x0007);
    send_frame(n);

    while (1) {
        /* Button press triggers a scan; here poll a "scan request" from hub */
        int rlen = ble_osmp_recv(s_rx, sizeof(s_rx), 50 /*ms*/);
        if (rlen > 0) {
            uint8_t type, seq, plen;
            uint8_t payload[OSMP_MAX_PAYLOAD];
            if (osmp_decode(s_rx, (size_t)rlen, &type, &seq, payload, &plen)) {
                if (type == OSMP_PING) {
                    uint8_t pong[4]; uint32_t t = rtc_now_unix();
                    memcpy(pong, &t, 4);
                    size_t o = osmp_encode(s_tx, sizeof(s_tx), OSMP_PONG, seq, pong, 4);
                    send_frame(o);
                } else if (type == OSMP_OTA_CHUNK) {
                    /* forward to OTA handler */
                }
            }
        }

        /* Periodic Wi-Fi OTA check (when docked / charging) */
        if (battery_charging()) wifi_ota_check();

        /* Deep sleep between scans (ULP wakes on button) */
        __asm__ volatile ("waiti 0");
    }
    return 0;
}

/* Triggered by the scan button GPIO ISR */
void on_scan_button(void)
{
    run_scan();
}