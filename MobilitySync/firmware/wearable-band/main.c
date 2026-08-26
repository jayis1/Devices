#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"

int main(void) {
    unsigned hr_bpm = 96;
    unsigned hrv_proxy = 22;
    unsigned fall_confidence = 12;

    mb_mesh_config_t config = { .node_id = 0x0104, .slot_index = 4, .uplink_period_ms = 1000, .retries = 1 };
    mb_mesh_init(&config);
    mb_mesh_log_metric("hr_bpm", (float)hr_bpm);

    mb_frame_t frame = {0};
    frame.preamble = MB_PREAMBLE;
    frame.version = MB_VERSION;
    frame.message_type = MB_MSG_TELEMETRY;
    frame.node_type = MB_NODE_BAND;
    frame.source_id = 0x0104;
    frame.destination_id = 0x0001;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, MB_MAX_PAYLOAD,
                                             "hr=%u,hrv=%u,fall=%u,sos=0",
                                             hr_bpm, hrv_proxy, fall_confidence);
    mb_mesh_send(&frame);
    return 0;
}
