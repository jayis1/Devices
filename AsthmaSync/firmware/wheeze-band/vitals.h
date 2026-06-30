/**
 * AsthmaSync — Wheeze Band Vitals Header
 *
 * License: MIT
 */

#ifndef VITALS_H
#define VITALS_H

#include "../common/protocol.h"
#include <stdint.h>

/* Vitals event types */
typedef enum {
    VITALS_EVENT_NORMAL    = 0,
    VITALS_EVENT_SPO2_LOW  = 1,
    VITALS_EVENT_HRV_DROP  = 2,
    VITALS_EVENT_HR_HIGH   = 3,
} vitals_event_t;

/** Callback type for vital sign alerts. */
typedef void (*vitals_callback_t)(vitals_event_t event, const vitals_t *vitals);

/** Initialize PPG (MAX30101) + temperature (TMP117) sensors. */
int vitals_init(void);

/** Process a PPG sample (called at 100 Hz from MAX30101 interrupt). */
void vitals_on_sample(int32_t red, int32_t ir);

/** Read skin temperature from TMP117. */
int tmp117_read_temp(float *temp_c);

/** Pack current vitals into protocol struct. */
int vitals_pack(vitals_t *out);

/** Register callback for vital sign alerts. */
void vitals_set_callback(vitals_callback_t cb);

#endif /* VITALS_H */