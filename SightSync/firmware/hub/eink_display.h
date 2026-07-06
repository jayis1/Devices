/**
 * SightSync Vision Hub — E-ink Display Driver (SSD1680, 2.9", 296×128)
 * License: MIT
 */

#ifndef EINK_DISPLAY_H
#define EINK_DISPLAY_H

#include "alert_engine.h"  /* for hub_state_t */

void eink_display_init(void);
void eink_display_update(const void *state);  /* hub_state_t* */
void eink_display_clear(void);

#endif /* EINK_DISPLAY_H */