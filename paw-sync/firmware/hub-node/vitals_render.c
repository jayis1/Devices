/*
 * vitals_render.c — PawSync Hub TFT display renderer
 *
 * Renders the pet's vitals, activity timeline, and wellness score on
 * the ILI9488 3.5" TFT (320×480). Uses SPI0 at 24MHz.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include "paw_protocol.h"

/* ILI9488 display dimensions */
#define TFT_W  320
#define TFT_H  480

/* Color helpers (RGB565) */
#define COLOR_BG       0x0000  /* black */
#define COLOR_TEXT     0xFFFF  /* white */
#define COLOR_GREEN    0x07E0
#define COLOR_YELLOW   0xFFE0
#define COLOR_RED      0xF800
#define COLOR_BLUE     0x001F
#define COLOR_CYAN     0x07FF
#define COLOR_GRAY     0x4208

/* Simple framebuffer (backing store) */
static uint16_t fb[TFT_W * TFT_H / 4];  /* quarter-res for memory */

/* ---- Stub display interface (in production: ILI9488 driver) ---- */
void tft_init(int spi_inst) { (void)spi_inst; }
void tft_fill(uint16_t color) { (void)color; }
void tft_draw_text(int x, int y, const char *text, uint16_t color) {
    (void)x; (void)y; (void)text; (void)color;
}
void tft_draw_rect(int x, int y, int w, int h, uint16_t color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
void tft_fill_rect(int x, int y, int w, int h, uint16_t color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
void tft_draw_circle(int cx, int cy, int r, uint16_t color) {
    (void)cx; (void)cy; (void)r; (void)color;
}
void tft_fill_circle(int cx, int cy, int r, uint16_t color) {
    (void)cx; (void)cy; (void)r; (void)color;
}
void tft_flush(void) {}

/* ---- Wellness score gauge ---- */
void render_wellness_gauge(uint8_t wellness_score)
{
    int cx = TFT_W / 2;
    int cy = 80;
    int r = 50;

    uint16_t color;
    if (wellness_score >= 70)      color = COLOR_GREEN;
    else if (wellness_score >= 50) color = COLOR_YELLOW;
    else                            color = COLOR_RED;

    tft_fill_rect(0, 0, TFT_W, 170, COLOR_BG);
    tft_fill_circle(cx, cy, r, COLOR_GRAY);
    tft_fill_circle(cx, cy, r - 4, COLOR_BG);

    /* Draw arc proportional to wellness (0-100 → 0-360°) */
    int angle = (wellness_score * 360) / 100;
    for (int a = 0; a < angle; a += 3) {
        float rad = (a - 90) * 3.14159f / 180.0f;
        int px = cx + (int)(cosf(rad) * (r - 2));
        int py = cy + (int)(sinf(rad) * (r - 2));
        tft_draw_circle(px, py, 3, color);
    }

    /* Score text */
    char buf[16];
    snprintf(buf, sizeof(buf), "%u%%", wellness_score);
    tft_draw_text(cx - 20, cy - 8, buf, color);
    tft_draw_text(cx - 30, cy + 14, "WELLNESS", COLOR_TEXT);
}

/* ---- Vitals display ---- */
void render_vitals(uint8_t hr, uint16_t hrv, int16_t temp_centic,
                   uint8_t activity_class)
{
    int y = 190;
    tft_fill_rect(0, y, TFT_W, 140, COLOR_BG);

    char buf[64];

    /* Heart rate */
    snprintf(buf, sizeof(buf), "HR:   %u bpm", hr);
    tft_draw_text(20, y + 10, buf, COLOR_RED);
    /* Heart icon */
    tft_fill_circle(15, y + 14, 4, COLOR_RED);

    /* HRV */
    snprintf(buf, sizeof(buf), "HRV:  %u.%02u ms", hrv / 100, hrv % 100);
    tft_draw_text(20, y + 35, buf, COLOR_CYAN);

    /* Temperature */
    int temp_c = temp_centic / 100;
    int temp_f = abs(temp_centic) % 100;
    snprintf(buf, sizeof(buf), "Temp: %d.%02d°C", temp_c, temp_f);
    tft_draw_text(20, y + 60, buf, COLOR_YELLOW);

    /* Activity */
    const char *act_name = "unknown";
    if (activity_class < 9)
        act_name = PAW_ACTIVITY_NAMES[activity_class];
    snprintf(buf, sizeof(buf), "Now:  %s", act_name);
    tft_draw_text(20, y + 85, buf, COLOR_GREEN);
}

/* ---- Activity timeline (mini bar chart) ---- */
void render_activity_timeline(const uint8_t *activities, int n)
{
    int y = 340;
    tft_fill_rect(0, y, TFT_W, 120, COLOR_BG);
    tft_draw_text(10, y, "ACTIVITY (last 2h)", COLOR_GRAY);

    int bar_w = (TFT_W - 20) / n;
    for (int i = 0; i < n && i < 24; i++) {
        uint16_t color;
        switch (activities[i]) {
            case 0: case 3: color = COLOR_GRAY;   break; /* rest/sleep */
            case 1:          color = COLOR_GREEN;   break; /* walk */
            case 2:          color = COLOR_CYAN;    break; /* run */
            case 4:          color = COLOR_YELLOW;  break; /* scratch */
            case 5:          color = COLOR_RED;     break; /* headshake */
            case 8:          color = COLOR_BLUE;    break; /* play */
            default:         color = COLOR_GRAY;   break;
        }
        tft_fill_rect(10 + i * bar_w, y + 20, bar_w - 1, 15, color);
    }
}

/* ---- Feeding summary ---- */
void render_feeding(uint16_t dispensed, uint16_t consumed, uint16_t water)
{
    int y = 440;
    char buf[48];
    snprintf(buf, sizeof(buf), "Food: %u/%ug  Water: %uml",
             consumed, dispensed, water);
    tft_draw_text(10, y, buf, COLOR_TEXT);
}

/* ---- Alert banner ---- */
void render_alert(const char *message)
{
    tft_fill_rect(0, 0, TFT_W, 20, COLOR_RED);
    tft_draw_text(5, 4, message, COLOR_TEXT);
}

/* ---- Full render cycle ---- */
void render_pet_vitals(uint8_t hr, uint16_t hrv, int16_t temp,
                       uint8_t activity, uint8_t wellness)
{
    tft_fill(COLOR_BG);
    render_wellness_gauge(wellness);
    render_vitals(hr, hrv, temp, activity);
    /* render_activity_timeline(...) — called with ring buffer data */
    /* render_feeding(...) — called with feeder data */
    tft_flush();
}