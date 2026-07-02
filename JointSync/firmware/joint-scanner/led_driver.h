/**
 * JointSync Joint Scanner — LED Driver Interface
 *
 * License: MIT
 */

#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <stdint.h>

typedef enum {
    LED_WHITE = 0,
    LED_UV    = 1,
    LED_NIR   = 2,
} led_type_t;

void led_driver_init(void);
void led_driver_set(led_type_t type, uint8_t brightness);
void led_driver_off(void);
void led_driver_flash(led_type_t type, uint16_t duration_ms);

#endif /* LED_DRIVER_H */