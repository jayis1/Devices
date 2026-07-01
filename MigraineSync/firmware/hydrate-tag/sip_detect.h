/**
 * MigraineSync — Hydrate Tag Sip Detection Header
 * ===============================================
 * License: MIT
 */

#ifndef HYDRATE_SIP_DETECT_H
#define HYDRATE_SIP_DETECT_H

#include <stdint.h>

typedef enum {
    SIP_EVENT_NONE   = 0,
    SIP_EVENT_SIP    = 1,   /* normal sip (5-50 ml) */
    SIP_EVENT_GULP   = 2,   /* large gulp (>50 ml) */
    SIP_EVENT_REFILL = 3,   /* bottle refilled (weight increased) */
    SIP_EVENT_SPILL  = 4,   /* weight decreased without tilt (spill) */
} sip_event_t;

/**
 * Initialize sip detection (LSM6DSO + HX711).
 */
int sip_detect_init(void);

/**
 * Poll for sip events. Call when tilt interrupt fires.
 */
sip_event_t sip_detect_poll(void);

/**
 * Get cumulative daily intake in ml.
 */
float sip_get_daily_intake_ml(void);

/**
 * Get sip count since last reset.
 */
uint8_t sip_get_sip_count(void);

/**
 * Get current bottle weight in grams.
 */
float sip_get_bottle_weight_g(void);

/**
 * Reset daily counters (call at midnight).
 */
void sip_reset_daily(void);

#endif /* HYDRATE_SIP_DETECT_H */