/**
 * SightSync Eye Tag — Blink Detection via IR Reflectance
 *
 * Samples an IR LED + photodiode at 50 Hz, detects blink events
 * using matched-filter template correlation, and computes
 * a 2-second sliding window blink rate.
 *
 * License: MIT
 */

#ifndef BLINK_DETECT_H
#define BLINK_DETECT_H

#include <stdint.h>

void blink_detect_init(uint8_t ir_led_pin, uint8_t photodiode_pin);
void blink_detect_sample(void);
void blink_detect_set_mode(uint8_t mode);  /* 0=work,1=rest,2=child,3=sleep */
uint8_t blink_detect_get_bpm(void);
uint8_t blink_detect_get_confidence(void);
uint8_t blink_detect_get_quality(void);
uint8_t blink_detect_get_ir_amplitude(void);
uint8_t blink_detect_get_battery_pct(void);
void blink_detect_accumulate_blue_dose(uint16_t ch0, uint16_t ch1);

#endif /* BLINK_DETECT_H */