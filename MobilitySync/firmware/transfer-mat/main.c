#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"

static float asymmetry_pct(float left_kg, float right_kg) {
    float total = left_kg + right_kg;
    if (total <= 0.001f) {
        return 0.0f;
    }
    float delta = left_kg - right_kg;
    if (delta < 0) {
        delta = -delta;
    }
    return (delta / total) * 100.0f;
}

int main(void) {
    float left_kg = 28.0f;
    float right_kg = 18.5f;
    float asym = asymmetry_pct(left_kg, right_kg);

    mb_mesh_config_t config = { .node_id = 0x0102, .slot_index = 2, .uplink_period_ms = 500, .retries = 2 };
    mb_mesh_init(&config);
    mb_mesh_log_metric("transfer_asymmetry_pct", asym);

    mb_frame_t frame = {0};
    frame.preamble = MB_PREAMBLE;
    frame.version = MB_VERSION;
    frame.message_type = asym > 18.0f ? MB_MSG_ALERT : MB_MSG_TELEMETRY;
    frame.node_type = MB_NODE_TRANSFER_MAT;
    frame.source_id = 0x0102;
    frame.destination_id = 0x0001;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, MB_MAX_PAYLOAD,
                                             "left=%.1f,right=%.1f,asym=%.1f,retries=%u",
                                             left_kg, right_kg, asym, asym > 18.0f ? 1u : 0u);
    mb_mesh_send(&frame);
    return 0;
}
