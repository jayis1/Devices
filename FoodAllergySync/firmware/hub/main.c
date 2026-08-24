#include <stdio.h>
#include <string.h>
#include "../common/protocol.h"
#include "../common/mesh.h"

static void publish_summary(void) {
    puts("[hub] publishing readiness summary to edge dashboard");
}

int main(void) {
    fas_mesh_config_t config = {
        .node_id = 0x0001,
        .slot_index = 0,
        .uplink_period_ms = 1000,
        .retries = 3
    };
    fas_mesh_init(&config);
    publish_summary();

    fas_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.preamble = FAS_PREAMBLE;
    frame.version = FAS_VERSION;
    frame.message_type = FAS_MSG_COMMAND;
    frame.node_type = FAS_NODE_HUB;
    frame.source_id = 0x0001;
    frame.destination_id = 0x0012;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, FAS_MAX_PAYLOAD, "calibrate_strip_reader");
    fas_mesh_send(&frame);
    puts("[hub] command queued");
    return 0;
}
