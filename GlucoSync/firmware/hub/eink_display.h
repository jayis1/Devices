#ifndef GLUCOSYNC_EINK_DISPLAY_H
#define GLUCOSYNC_EINK_DISPLAY_H

#include <stdint.h>

/**
 * UC8151D e-ink display driver (2.9", 296×128).
 * Always-on glucose display, zero power when static.
 */

void eink_display_init(void);
void eink_display_boot_screen(void);
void eink_display_glucose(uint16_t glucose_mgdl, int16_t trend, uint16_t forecast_30);
void eink_display_alert(const char *message);
void eink_display_clear(void);
void eink_display_tir_ring(uint8_t tir_percent);

#endif