#ifndef GLUCOSYNC_INJECTION_DETECT_H
#define GLUCOSYNC_INJECTION_DETECT_H

#include <stdint.h>

typedef struct {
    uint8_t  confidence;     /* 0-100 */
    uint16_t duration_ms;    /* injection duration */
    uint32_t timestamp;      /* ms since boot */
} injection_event_t;

typedef void (*injection_cb)(const injection_event_t *event);

void injection_detect_init(injection_cb callback);
void injection_detect_process_imu(void);

/* State machine states (for debugging/testing) */
typedef enum {
    INJ_STATE_IDLE = 0,
    INJ_STATE_PICKUP,
    INJ_STATE_ORIENT,
    INJ_STATE_INSERT,
    INJ_STATE_INJECT,
    INJ_STATE_HOLD,
    INJ_STATE_DONE,
} inj_state_t;

inj_state_t injection_detect_get_state(void);

#endif