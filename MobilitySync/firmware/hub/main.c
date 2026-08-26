#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"

int main(void) {
    mb_mesh_config_t config = { .node_id = 0x0001, .slot_index = 0, .uplink_period_ms = 1000, .retries = 3 };
    mb_mesh_init(&config);
    mb_mesh_log_metric("transfer_safety_score", 82.5f);

    mb_frame_t frame = {0};
    frame.preamble = MB_PREAMBLE;
    frame.version = MB_VERSION;
    frame.message_type = MB_MSG_COMMAND;
    frame.node_type = MB_NODE_HUB;
    frame.source_id = 0x0001;
    frame.destination_id = 0x0103;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, MB_MAX_PAYLOAD, "open_door:hall_bath:soft");
    mb_mesh_send(&frame);
    puts("[hub] mobility summary published");
    return 0;
}
