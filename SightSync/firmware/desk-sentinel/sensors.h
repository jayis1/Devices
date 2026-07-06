/**
 * SightSync Desk Sentinel — Sensor Drivers
 * License: MIT
 */

#ifndef DESK_SENSORS_H
#define DESK_SENSORS_H

#include <stdint.h>

void     sensors_init(void);
uint16_t sensors_read_distance(uint8_t *quality);
uint16_t sensors_read_lux(void);
void     sensors_read_rgbc(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c);
uint16_t sensors_estimate_cct(uint16_t r, uint16_t g, uint16_t b, uint16_t c);
uint16_t sensors_read_blue_light(void);

#endif /* DESK_SENSORS_H */