#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

int main(void) {
    wsync_tap_event_t event = {
        .tap_id = 1,
        .flow_pulses = 42,
        .temp_c_x100 = 1188,
        .uv_lux = 612,
        .filter_days_remaining = 23,
        .door_open = 0,
        .advisory_ack = 0,
        .reserved = 0,
    };
    printf("tap id=%u flow=%u temp=%.2f uv=%u filter_days=%u\n",
           event.tap_id,
           event.flow_pulses,
           event.temp_c_x100 / 100.0,
           event.uv_lux,
           event.filter_days_remaining);
    return 0;
}
