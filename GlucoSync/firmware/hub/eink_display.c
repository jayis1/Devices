/**
 * UC8151D e-ink display driver.
 * Production: uses ESP-IDF SPI driver on GPIO4-9.
 * License: MIT
 */

#include "eink_display.h"
#include <stdio.h>
#include <string.h>

/* UC8151D SPI commands */
#define UC8151D_CMD_PANEL_SETTING   0x00
#define UC8151D_CMD_POWER_SETTING   0x01
#define UC8151D_CMD_POWER_ON         0x04
#define UC8151D_CMD_BOOSTER_SOFT_START 0x0C
#define UC8151D_CMD_DISPLAY_REFRESH  0x12
#define UC8151D_CMD_PROGRAM_START    0x14

static bool g_initialized = false;

void eink_display_init(void)
{
    /* Production: SPI init on GPIO4(SCK)/GPIO5(DIN)/GPIO6(CS)/GPIO7(DC)/GPIO8(RST)
     * Send UC8151D init sequence:
     * - Panel setting: 0x0F (296×128)
     * - Power setting: 0x03 0x00 0x2b 0x2b
     * - Booster soft start: 0x17 0x17 0x17
     * - Power on, wait for busy
     */
    g_initialized = true;
}

void eink_display_boot_screen(void)
{
    if (!g_initialized) return;
    /* Production: render "GlucoSync" logo + "Pairing..." to frame buffer,
     * send to display via SPI */
}

void eink_display_glucose(uint16_t glucose_mgdl, int16_t trend, uint16_t forecast_30)
{
    if (!g_initialized) return;

    /* Production: render to frame buffer:
     * - Large glucose number (center, 48pt font)
     * - Trend arrow (↑/→/↓ or ↗↘)
     * - 30-min forecast (small, below)
     * - Time-in-range ring (if available)
     * - Timestamp
     * Send via SPI + refresh
     */

    (void)glucose_mgdl;
    (void)trend;
    (void)forecast_30;
}

void eink_display_alert(const char *message)
{
    if (!g_initialized || message == NULL) return;
    /* Production: render alert message in inverse video (black bg, white text) */
    (void)message;
}

void eink_display_clear(void)
{
    if (!g_initialized) return;
    /* Production: fill frame buffer with white, refresh */
}

void eink_display_tir_ring(uint8_t tir_percent)
{
    if (!g_initialized) return;
    /* Production: draw ring showing TIR percentage */
    (void)tir_percent;
}