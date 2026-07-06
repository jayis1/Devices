/**
 * SightSync Smart Lamp Node — TLC5971 LED Driver
 * License: MIT
 */

#ifndef TLC5971_H
#define TLC5971_H

#include <stdint.h>

void tlc5971_init(uint8_t sck_pin, uint8_t mosi_pin, uint8_t latch_pin, uint8_t blank_pin);
void tlc5971_set_all(const uint16_t channels[16]);
void tlc5971_set_channel(uint8_t ch, uint16_t value);
void tlc5971_update(void);

#endif /* TLC5971_H */