/*
 * heatmap_render.c — SoleGuard foot heat-map TFT renderer (ILI9488)
 *
 * Draws two foot outlines (left + right) with 6 pressure zones each,
 * color-coded by pressure (green->yellow->red) and temperature
 * asymmetry (blue overlay for cool, red overlay for hot zones).
 * Shows the risk score bar at the top.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ili9488.h"
#include "sole_protocol.h"
#include <math.h>

#define TFT_W 320
#define TFT_H 480

/* Zone bounding boxes within each foot outline (normalized 0..1 within foot rect) */
typedef struct { float x0, y0, x1, y1; } zone_rect_t;
static const zone_rect_t foot_zones[6] = {
    { 0.30f, 0.80f, 0.70f, 1.00f },  /* heel */
    { 0.25f, 0.50f, 0.75f, 0.80f },  /* midfoot */
    { 0.55f, 0.30f, 0.80f, 0.50f },  /* metatarsal 1 (medial) */
    { 0.25f, 0.30f, 0.55f, 0.50f },  /* metatarsals 2-5 (lateral) */
    { 0.50f, 0.15f, 0.70f, 0.30f },  /* hallux */
    { 0.25f, 0.15f, 0.50f, 0.30f },  /* lesser toes */
};

static uint16_t pressure_color(uint8_t q)
{
    /* 0 -> green, 128 -> yellow, 255 -> red */
    if (q < 128) {
        uint8_t g = 255;
        uint8_t r = (uint8_t)((uint16_t)q * 2);
        return ili9488_rgb565(r, g, 0);
    } else {
        uint8_t r = 255;
        uint8_t g = (uint8_t)(255 - ((uint16_t)(q - 128) * 2));
        return ili9488_rgb565(r, g, 0);
    }
}

static uint16_t temp_asym_color(float asym_degC)
{
    /* >2.2C -> bright red overlay; <0.5C -> no overlay (transparent green-ish) */
    if (asym_degC > 2.2f) return ili9488_rgb565(255, 0, 0);
    if (asym_degC > 1.0f) return ili9488_rgb565(255, 80, 0);
    return 0; /* no overlay */
}

static void draw_foot(int origin_x, int origin_y, int foot_w, int foot_h,
                      const uint8_t pressure[24], const int16_t temp[8],
                      const int16_t temp_other[8], uint8_t risk)
{
    /* Foot outline (rounded rectangle approximation) */
    ili9488_fill_roundrect(origin_x, origin_y, foot_w, foot_h, 20,
                           ili9488_rgb565(40, 40, 40));

    for (int z = 0; z < 6; z++) {
        uint8_t peak = sole_zone_peak_pressure(pressure, (sole_zone_t)z);
        uint16_t col = pressure_color(peak);
        /* temperature asymmetry overlay for this zone (use avg of zone's sensors) */
        float asym = 0;
        int n = (z < 4) ? 2 : 2; /* map zones to temp sensors (simplified) */
        for (int i = 0; i < 2; i++) {
            int ti = z * 2 + i; if (ti > 7) ti = 7;
            float d = (temp[ti] - temp_other[ti]) / 100.0f;
            if (d < 0) d = -d;
            asym += d;
        }
        asym /= 2.0f;
        uint16_t ovr = temp_asym_color(asym);
        if (ovr) col = ovr; /* asymmetry takes visual priority */

        int zx = origin_x + (int)(foot_zones[z].x0 * foot_w);
        int zy = origin_y + (int)(foot_zones[z].y0 * foot_h);
        int zw = (int)((foot_zones[z].x1 - foot_zones[z].x0) * foot_w);
        int zh = (int)((foot_zones[z].y1 - foot_zones[z].y0) * foot_h);
        ili9488_fill_rect(zx, zy, zw, zh, col);
    }
    /* Risk label */
    char label[24];
    snprintf(label, sizeof(label), "Risk: %d%%", risk);
    ili9488_draw_text(origin_x + 5, origin_y + foot_h + 5, label,
                      risk > 65 ? ili9488_rgb565(255, 0, 0) :
                      risk > 40 ? ili9488_rgb565(255, 200, 0) :
                                  ili9488_rgb565(0, 255, 0));
}

void render_foot_heatmap(const uint8_t pL[24], const uint8_t pR[24],
                         const int16_t tL[8], const int16_t tR[8],
                         uint8_t riskL, uint8_t riskR)
{
    ili9488_fill_screen(ili9488_rgb565(10, 10, 30));
    ili9488_draw_text(80, 10, "SoleGuard Foot Map", ili9488_rgb565(255, 255, 255));

    /* Left foot on left half, right foot on right half */
    int fw = 130, fh = 260;
    draw_foot(30,  80, fw, fh, pL, tL, tR, riskL);
    draw_foot(160, 80, fw, fh, pR, tR, tL, riskR);

    /* Status bar at bottom */
    const char *status = (riskL > 65 || riskR > 65) ? "HIGH RISK — OFFLOAD" :
                         (riskL > 40 || riskR > 40) ? "WATCH" : "OK";
    uint16_t scol = (riskL > 65 || riskR > 65) ? ili9488_rgb565(255, 0, 0) :
                     (riskL > 40 || riskR > 40) ? ili9488_rgb565(255, 200, 0) :
                                                   ili9488_rgb565(0, 255, 0);
    ili9488_fill_rect(0, TFT_H - 30, TFT_W, 30, scol);
    ili9488_draw_text(100, TFT_H - 22, (char *)status, ili9488_rgb565(0, 0, 0));
}