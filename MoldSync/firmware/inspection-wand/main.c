#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"
#include "../common/protocol.h"

static int estimate_damp_pixels(int thermal_cold_pixels, int conductivity_score) {
    return thermal_cold_pixels * 2 + conductivity_score / 4;
}

int main(void) {
    ms_mesh_config_t config = {
        .node_id = 0x0042,
        .slot_index = 5,
        .uplink_period_ms = 0,
        .retries = 3,
    };
    ms_mesh_init(&config);

    int damp_score = estimate_damp_pixels(43, 68);
    printf("[inspection-wand] damp score=%d\n", damp_score);

    ms_frame_t frame = {0};
    frame.preamble = MS_PREAMBLE;
    frame.version = MS_VERSION;
    frame.message_type = MS_MSG_EVENT;
    frame.node_type = MS_NODE_INSPECTION_WAND;
    frame.source_id = 0x0042;
    frame.destination_id = 0x0001;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, MS_MAX_PAYLOAD,
                                             "damp_score=%d", damp_score);
    ms_mesh_send(&frame);
    return 0;
}
