#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../common/protocol.h"

typedef struct {
    int16_t tilt_mdps;
    uint16_t lift_peak_mg;
    bool inversion_detected;
} motion_window_t;

static uint8_t classify_event(const motion_window_t *window) {
    if (window->lift_peak_mg > 2400u && window->inversion_detected) {
        return WS_EVENT_PICKUP_CONFIRMED;
    }
    if (window->lift_peak_mg > 300u) {
        return WS_EVENT_CURB_PLACED;
    }
    return WS_EVENT_MISSED_PICKUP;
}

int main(void) {
    motion_window_t window = {
        .tilt_mdps = -680,
        .lift_peak_mg = 2840u,
        .inversion_detected = true,
    };

    ws_pickup_event_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.event_type = classify_event(&window);
    evt.tilt_mdps = window.tilt_mdps;
    evt.lift_peak_mg = window.lift_peak_mg;
    evt.solar_mv = 6120u;
    evt.battery_mv = 3310u;
    evt.next_pickup_epoch = 1790000000u;

    printf("beacon event=%u lift=%u battery=%u solar=%u\n",
           evt.event_type, evt.lift_peak_mg, evt.battery_mv, evt.solar_mv);
    return 0;
}
