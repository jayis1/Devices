#include <stdio.h>
#include <stdbool.h>
#include "../common/mesh.h"
#include "../common/protocol.h"

static bool thermal_safe(float left_c, float right_c, float ntc_c) {
    float spread = left_c > right_c ? (left_c - right_c) : (right_c - left_c);
    return left_c < 43.0f && right_c < 43.0f && ntc_c < 44.0f && spread < 1.2f;
}

int main(void) {
    mesh_config_t cfg = { .node_id = 0x2003, .channel = 3, .tx_power_dbm = 12 };
    mesh_init(&cfg);

    float left_c = 40.8f;
    float right_c = 40.5f;
    float ntc_c = 40.7f;
    uint8_t fault = thermal_safe(left_c, right_c, ntc_c) ? 0 : 1;

    uint8_t payload[7] = {
        2,
        (uint8_t)((int)(left_c * 100) >> 8),
        (uint8_t)((int)(left_c * 100) & 0xFF),
        (uint8_t)((int)(right_c * 100) >> 8),
        (uint8_t)((int)(right_c * 100) & 0xFF),
        82,
        fault
    };

    psync_frame_t frame;
    psync_build_frame(&frame, PSYNC_MSG_TELEMETRY, cfg.node_id, 0x1001, 1724390400UL, 0, payload, sizeof(payload));
    mesh_send_frame(&frame);
    printf("relief_belt left=%.2f right=%.2f safe=%u\n", left_c, right_c, fault == 0);
    return 0;
}
