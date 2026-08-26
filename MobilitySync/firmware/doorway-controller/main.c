#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"

int main(void) {
    float range_m = 1.42f;
    int obstruction = 0;

    mb_mesh_config_t config = { .node_id = 0x0103, .slot_index = 3, .uplink_period_ms = 250, .retries = 3 };
    mb_mesh_init(&config);
    mb_mesh_log_metric("doorway_range_m", range_m);

    mb_frame_t frame = {0};
    frame.preamble = MB_PREAMBLE;
    frame.version = MB_VERSION;
    frame.message_type = obstruction ? MB_MSG_ALERT : MB_MSG_EVENT;
    frame.node_type = MB_NODE_DOORWAY;
    frame.source_id = 0x0103;
    frame.destination_id = 0x0001;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, MB_MAX_PAYLOAD,
                                             "door=hall_bath,range=%.2f,action=%s",
                                             range_m, obstruction ? "hold" : "open_soft");
    mb_mesh_send(&frame);
    return 0;
}
