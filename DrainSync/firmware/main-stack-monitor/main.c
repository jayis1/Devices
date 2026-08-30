#include <stdio.h>
#include <stdint.h>
#include "../common/protocol.h"

static int16_t estimate_pressure_delta(uint16_t level_mm, uint16_t surges) {
    return (int16_t)((int32_t)level_mm / 6 - (int32_t)surges * 3);
}

int main(void) {
    ds_stack_status_t status = {
        .level_mm = 410u,
        .diff_pressure_pa = estimate_pressure_delta(410u, 18u),
        .surge_count = 18u,
        .battery_mv = 7680u,
    };

    ds_frame_t frame;
    ds_init_frame(&frame, DS_MSG_TELEMETRY, 0x1301u, 0x1000u, 0x55667788u, 12u);
    ds_encode_stack_status(&frame, &status);
    printf("stack level=%u pressure=%d crc=%u\n", status.level_mm, status.diff_pressure_pa, frame.crc);
    return 0;
}
