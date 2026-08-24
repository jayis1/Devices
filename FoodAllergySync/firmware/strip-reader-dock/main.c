#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../common/protocol.h"
#include "../common/mesh.h"

static float compute_ratio(float test_line, float control_line) {
    if (control_line < 0.001f) {
        return 0.0f;
    }
    return test_line / control_line;
}

int main(void) {
    float test_line = 0.42f;
    float control_line = 0.95f;
    float ratio = compute_ratio(test_line, control_line);

    fas_mesh_config_t config = {.node_id = 0x0012, .slot_index = 2, .uplink_period_ms = 10000, .retries = 3};
    fas_mesh_init(&config);

    fas_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.preamble = FAS_PREAMBLE;
    frame.version = FAS_VERSION;
    frame.message_type = ratio > 0.30f ? FAS_MSG_ALERT : FAS_MSG_EVENT;
    frame.node_type = FAS_NODE_STRIP_READER;
    frame.source_id = 0x0012;
    frame.destination_id = 0x0001;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, FAS_MAX_PAYLOAD, "ratio=%.3f", ratio);
    fas_mesh_send(&frame);

    printf("[strip-reader] ratio=%.3f\n", ratio);
    return 0;
}
