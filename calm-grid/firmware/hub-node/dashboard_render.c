/*
 * dashboard_render.c — CalmGrid Hub TFT display renderer
 *
 * Renders the stress gauge, breathing-guide animation, and episode
 * timeline on the ILI9488 3.5" TFT (320×480). Uses SPI0 at 24MHz.
 *
 * Breathing animation: an expanding/contracting circle synced to the
 * breathing pattern (inhale = expand, exhale = contract).
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "calm_protocol.h"

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
#define COLOR_PURPLE   0x780F
#define COLOR_ORANGE   0xFD20

/* Simple framebuffer (quarter-res for memory) */
static uint16_t fb[TFT_W * TFT_H / 4];

/* ---- Stub display interface (in production: ILI9488 driver) ---- */
static void tft_set_pixel(int x, int y, uint16_t color)
{
    (void)x; (void)y; (void)color;
}

static void tft_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    (void)x; (void)y; (void)w; (void)h; (void)color;
}

static void tft_draw_text(int x, int y, const char *text, uint16_t color,
                          int size)
{
    (void)x; (void)y; (void)text; (void)color; (void)size;
}

static void tft_draw_circle(int cx, int cy, int r, uint16_t color)
{
    (void)cx; (void)cy; (void)r; (void)color;
}

static void tft_fill_circle(int cx, int cy, int r, uint16_t color)
{
    (void)cx; (void)cy; (void)r; (void)color;
}

/* ---- Color for stress level ---- */
static uint16_t stress_color(uint8_t stress)
{
    if (stress < 30) return COLOR_GREEN;
    if (stress < 50) return COLOR_YELLOW;
    if (stress < 70) return COLOR_ORANGE;
    return COLOR_RED;
}

/* ---- Render main stress gauge ---- */
void dashboard_render_stress(uint8_t stress, uint8_t burnout,
                              uint8_t recovery)
{
    /* Clear */
    tft_fill_rect(0, 0, TFT_W, TFT_H, COLOR_BG);

    /* Header */
    tft_draw_text(40, 10, "CalmGrid", COLOR_CYAN, 3);
    tft_draw_text(30, 50, "Stress Monitor", COLOR_GRAY, 2);

    /* Main stress gauge (large circle) */
    uint16_t col = stress_color(stress);
    int cx = TFT_W / 2;
    int cy = 180;
    int radius = 60 + stress / 3;  /* size reflects magnitude */

    tft_draw_circle(cx, cy, 70, COLOR_GRAY);
    tft_fill_circle(cx, cy, radius, col);

    /* Stress score text */
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", stress);
    tft_draw_text(cx - 20, cy - 10, buf, COLOR_TEXT, 4);
    tft_draw_text(cx - 35, cy + 20, "STRESS", COLOR_TEXT, 2);

    /* Burnout risk bar */
    tft_draw_text(20, 300, "Burnout Risk (14d)", COLOR_GRAY, 2);
    tft_fill_rect(20, 325, 280, 20, COLOR_GRAY);
    tft_fill_rect(20, 325, 280 * burnout / 100, 20, stress_color(burnout));
    snprintf(buf, sizeof(buf), "%u%%", burnout);
    tft_draw_text(230, 327, buf, COLOR_TEXT, 2);

    /* Recovery score bar */
    tft_draw_text(20, 360, "Recovery Score", COLOR_GRAY, 2);
    tft_fill_rect(20, 385, 280, 20, COLOR_GRAY);
    tft_fill_rect(20, 385, 280 * recovery / 100, 20, COLOR_GREEN);
    snprintf(buf, sizeof(buf), "%u%%", recovery);
    tft_draw_text(230, 387, buf, COLOR_TEXT, 2);

    /* Status text */
    const char *status;
    if (stress < 30) status = "Calm - well regulated";
    else if (stress < 50) status = "Normal range";
    else if (stress < 70) status = "Elevated - take a break";
    else status = "High stress - intervene now";
    tft_draw_text(20, 430, status, col, 2);
}

/* ---- Render breathing-guide animation ---- */
/* Expanding/contracting circle synced to breathing pattern */
void dashboard_render_breathing(uint8_t pattern, uint16_t elapsed_s,
                                 uint16_t duration_s)
{
    (void)pattern;
    (void)elapsed_s;
    (void)duration_s;

    /* Clear */
    tft_fill_rect(0, 0, TFT_W, TFT_H, COLOR_BG);

    /* Title */
    tft_draw_text(40, 10, "Breathe", COLOR_CYAN, 3);

    /* Compute breathing phase (simplified — in production, get from intervention.c) */
    /* 4-7-8 pattern: inhale 4, hold 7, exhale 8 */
    uint16_t cycle_len = 4 + 7 + 8;
    uint16_t cycle_pos = elapsed_s % cycle_len;

    int radius;
    const char *phase_text;
    uint16_t phase_color;

    if (cycle_pos < 4) {
        /* Inhale — expand */
        float t = (float)cycle_pos / 4.0f;
        radius = 30 + (int)(t * 50);
        phase_text = "Breathe In...";
        phase_color = COLOR_CYAN;
    } else if (cycle_pos < 11) {
        /* Hold */
        radius = 80;
        phase_text = "Hold";
        phase_color = COLOR_YELLOW;
    } else {
        /* Exhale — contract */
        float t = (float)(cycle_pos - 11) / 8.0f;
        radius = 80 - (int)(t * 50);
        phase_text = "Breathe Out...";
        phase_color = COLOR_PURPLE;
    }

    /* Draw breathing circle */
    int cx = TFT_W / 2;
    int cy = 200;
    tft_draw_circle(cx, cy, 90, COLOR_GRAY);  /* outer guide */
    tft_fill_circle(cx, cy, radius, phase_color);

    /* Phase text */
    tft_draw_text(80, 320, phase_text, COLOR_TEXT, 3);

    /* Progress bar */
    uint16_t progress = (elapsed_s * 280) / duration_s;
    tft_fill_rect(20, 380, 280, 10, COLOR_GRAY);
    tft_fill_rect(20, 380, progress, 10, COLOR_CYAN);

    /* Elapsed / total */
    char buf[32];
    snprintf(buf, sizeof(buf), "%us / %us", elapsed_s, duration_s);
    tft_draw_text(80, 410, buf, COLOR_GRAY, 2);
}