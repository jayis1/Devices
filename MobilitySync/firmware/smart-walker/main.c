#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"

static float compute_slip_score(float push_n, float wheel_rps) {
    return (wheel_rps > push_n * 0.2f) ? (wheel_rps - push_n * 0.2f) : 0.0f;
}

int main(void) {
    float left_force_n = 58.0f;
    float right_force_n = 61.0f;
    float wheel_rps = 2.8f;
    float slip = compute_slip_score((left_force_n + right_force_n) * 0.5f, wheel_rps);

    mb_mesh_config_t config = { .node_id = 0x0101, .slot_index = 1, .uplink_period_ms = 200, .retries = 2 };
    mb_mesh_init(&config);
    mb_mesh_log_metric("walker_slip_score", slip);

    mb_frame_t frame = {0};
    frame.preamble = MB_PREAMBLE;
    frame.version = MB_VERSION;
    frame.message_type = slip > 0.5f ? MB_MSG_ALERT : MB_MSG_TELEMETRY;
    frame.node_type = MB_NODE_WALKER;
    frame.source_id = 0x0101;
    frame.destination_id = 0x0001;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, MB_MAX_PAYLOAD,
                                             "forceL=%.1f,forceR=%.1f,slip=%.2f,brake=%s",
                                             left_force_n, right_force_n, slip,
                                             slip > 0.5f ? "pulse" : "released");
    mb_mesh_send(&frame);
    return 0;
}
