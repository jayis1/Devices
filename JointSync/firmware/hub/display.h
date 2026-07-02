/**
 * JointSync Hub — Display Interface
 *
 * License: MIT
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

void display_init(void);
void display_update_temp(uint16_t sender_id, float temp, float inflam_prob);
void display_update_pressure(float pressure_mmhg, uint8_t pump_state);
void display_update_heartbeat(uint8_t counter);

bool display_get_button1(void);
bool display_get_button2(void);
bool display_get_button3(void);

void display_next_page(void);

#endif /* DISPLAY_H */