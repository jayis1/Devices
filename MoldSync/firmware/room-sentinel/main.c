#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"
#include "../common/protocol.h"

static float compute_margin(float air_temp_c, float rh, float surface_temp_c) {
    float dew_point_approx = air_temp_c - ((100.0f - rh) / 5.0f);
    return surface_temp_c - dew_point_approx;
}

int main(void) {
    ms_mesh_config_t config = {
        .node_id = 0x0214,
        .slot_index = 2,
        .uplink_period_ms = 60000,
        .retries = 2,
    };
    ms_mesh_init(&config);

    float margin = compute_margin(24.1f, 78.2f, 21.0f);
    printf("[room-sentinel] condensation margin=%.2f C\n", margin);

    ms_frame_t frame = {0};
    frame.preamble = MS_PREAMBLE;
    frame.version = MS_VERSION;
    frame.message_type = MS_MSG_TELEMETRY;
    frame.node_type = MS_NODE_ROOM_SENTINEL;
    frame.source_id = 0x0214;
    frame.destination_id = 0x0001;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, MS_MAX_PAYLOAD,
                                             "bath-east|rh=782|margin=%d", (int)(margin * 100.0f));
    ms_mesh_send(&frame);
    return 0;
}
