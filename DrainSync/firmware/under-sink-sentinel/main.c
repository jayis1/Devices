#include <stdio.h>
#include <stdint.h>
#include "../common/protocol.h"

static uint16_t fake_turbulence(uint16_t drain_ms, uint16_t vib_rms) {
    return (uint16_t)((drain_ms / 12u) + (vib_rms / 4u));
}

int main(void) {
    ds_flow_summary_t summary = {
        .duration_ms = 1840u,
        .turbulence = fake_turbulence(1840u, 96u),
        .vibration_rms = 96u,
        .gas_index = 37u,
        .leak_flags = 0u,
        .temperature_c = 42u,
    };

    ds_frame_t frame;
    ds_init_frame(&frame, DS_MSG_TELEMETRY, 0x1101u, 0x1000u, 0x12345678u, 1u);
    ds_encode_flow_summary(&frame, &summary);
    printf("under-sink duration=%u turbulence=%u crc=%u\n", summary.duration_ms, summary.turbulence, frame.crc);
    return 0;
}
