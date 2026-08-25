#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"
#include "../common/protocol.h"

static void publish_summary(void) {
    puts("[hub] publishing mold risk summary to dashboard");
}

int main(void) {
    ms_mesh_config_t config = {
        .node_id = 0x0001,
        .slot_index = 0,
        .uplink_period_ms = 1000,
        .retries = 3,
    };
    ms_mesh_init(&config);
    publish_summary();

    ms_frame_t frame = {0};
    frame.preamble = MS_PREAMBLE;
    frame.version = MS_VERSION;
    frame.message_type = MS_MSG_COMMAND;
    frame.node_type = MS_NODE_HUB;
    frame.source_id = 0x0001;
    frame.destination_id = 0x0017;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, MS_MAX_PAYLOAD, "run_vent:1200");
    ms_mesh_send(&frame);
    return 0;
}
