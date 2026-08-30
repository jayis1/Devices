#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../common/protocol.h"

static uint16_t travel_time_ms(uint8_t from_pos, uint8_t to_pos) {
    uint8_t delta = (from_pos > to_pos) ? (uint8_t)(from_pos - to_pos) : (uint8_t)(to_pos - from_pos);
    return (uint16_t)(delta * 14u);
}

int main(void) {
    ds_actuator_status_t status = {
        .target_position = 100u,
        .measured_position = 98u,
        .motor_current_ma = 1280u,
        .travel_time_ms = travel_time_ms(0u, 100u),
        .fault_bits = 0u,
    };

    ds_frame_t frame;
    ds_init_frame(&frame, DS_MSG_TELEMETRY, 0x1401u, 0x1000u, 0x99887766u, 4u);
    ds_encode_actuator_status(&frame, &status);
    printf("actuator pos=%u current=%u crc=%u\n", status.measured_position, status.motor_current_ma, frame.crc);
    return 0;
}
