#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

static us_mat_event_t evaluate_transfer(uint16_t latency_ms, uint16_t sway, bool slip) {
    us_mat_event_t event = {0};
    event.transfer_latency_ms = latency_ms;
    event.sway_index = sway;
    event.left_load_pct = 47u;
    event.right_load_pct = 53u;
    event.left_temp_c_x100 = 3125;
    event.right_temp_c_x100 = 3190;
    event.trip_duration_s = 146u;
    event.slip_flag = slip ? 1u : 0u;
    return event;
}

int main(void) {
    us_mat_event_t event = evaluate_transfer(1820u, 68u, false);
    printf("night-mat latency_ms=%u sway=%u trip_s=%u slip=%u\n",
           event.transfer_latency_ms,
           event.sway_index,
           event.trip_duration_s,
           event.slip_flag);
    return 0;
}
