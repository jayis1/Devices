#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"
#include "../common/protocol.h"

static int choose_runtime_minutes(float rh, float target_rh) {
    float delta = rh - target_rh;
    if (delta <= 0.0f) return 0;
    if (delta < 5.0f) return 8;
    if (delta < 12.0f) return 15;
    return 25;
}

int main(void) {
    ms_mesh_config_t config = {
        .node_id = 0x0017,
        .slot_index = 3,
        .uplink_period_ms = 30000,
        .retries = 2,
    };
    ms_mesh_init(&config);

    int minutes = choose_runtime_minutes(78.2f, 55.0f);
    printf("[vent-controller] fan runtime=%d minutes\n", minutes);

    ms_frame_t frame = {0};
    frame.preamble = MS_PREAMBLE;
    frame.version = MS_VERSION;
    frame.message_type = MS_MSG_EVENT;
    frame.node_type = MS_NODE_VENT_CONTROLLER;
    frame.source_id = 0x0017;
    frame.destination_id = 0x0001;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, MS_MAX_PAYLOAD,
                                             "fan_runtime_min=%d", minutes);
    ms_mesh_send(&frame);
    return 0;
}
