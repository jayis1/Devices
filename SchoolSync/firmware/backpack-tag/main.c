#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

static ss_backpack_state_t sample_state(uint8_t tick) {
    ss_backpack_state_t s = {0};
    s.carry_state = tick < 2 ? SS_STATE_STATIONARY : SS_STATE_WORN;
    s.pocket_homework_open = (tick == 1);
    s.pocket_meds_open = 0;
    s.ack_pressed = (tick == 2);
    s.range_cm = (int16_t)(240 - tick * 60);
    s.accel_peak_mg = (int16_t)(120 + tick * 150);
    s.bag_temp_c_x100 = 2450;
    s.battery_mv = 3920;
    return s;
}

int main(void) {
    for (uint8_t tick = 0; tick < 4; ++tick) {
        ss_backpack_state_t s = sample_state(tick);
        printf("backpack carry=%u homework=%u ack=%u range_cm=%d accel=%d battery=%u\n",
               s.carry_state,
               s.pocket_homework_open,
               s.ack_pressed,
               s.range_cm,
               s.accel_peak_mg,
               s.battery_mv);
    }
    return 0;
}
