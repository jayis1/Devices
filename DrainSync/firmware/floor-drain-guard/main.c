#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../common/protocol.h"

static bool should_prime(uint16_t trap_depth_raw, uint16_t h2s_ppb) {
    return trap_depth_raw < 280u && h2s_ppb > 25u;
}

int main(void) {
    ds_trap_status_t status = {
        .trap_depth_raw = 240u,
        .h2s_ppb = 31u,
        .humidity_rh_x10 = 612u,
        .primer_cycles = 1u,
        .water_present = 0u,
    };

    ds_frame_t frame;
    ds_init_frame(&frame, DS_MSG_ALERT, 0x1201u, 0x1000u, 0x12340099u, 9u);
    ds_encode_trap_status(&frame, &status);
    printf("floor-drain prime=%s h2s=%u crc=%u\n", should_prime(status.trap_depth_raw, status.h2s_ppb) ? "yes" : "no", status.h2s_ppb, frame.crc);
    return 0;
}
