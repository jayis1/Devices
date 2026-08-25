#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"
#include "../common/protocol.h"

static int leak_confidence_ml(unsigned flow_ml_min, unsigned leak_adc, int temp_delta_centi) {
    int score = 20;
    if (flow_ml_min > 120) score += 35;
    if (leak_adc > 700) score += 35;
    if (temp_delta_centi < -250) score += 10;
    return score > 100 ? 100 : score;
}

int main(void) {
    ms_mesh_config_t config = {
        .node_id = 0x0031,
        .slot_index = 4,
        .uplink_period_ms = 15000,
        .retries = 3,
    };
    ms_mesh_init(&config);

    int confidence = leak_confidence_ml(145, 812, -310);
    printf("[plumbing-interlock] leak confidence=%d\n", confidence);

    ms_frame_t frame = {0};
    frame.preamble = MS_PREAMBLE;
    frame.version = MS_VERSION;
    frame.message_type = confidence >= 75 ? MS_MSG_ALERT : MS_MSG_TELEMETRY;
    frame.node_type = MS_NODE_PLUMBING_INTERLOCK;
    frame.source_id = 0x0031;
    frame.destination_id = 0x0001;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, MS_MAX_PAYLOAD,
                                             "leak_confidence=%d", confidence);
    ms_mesh_send(&frame);
    return 0;
}
