#include <stdio.h>
#include <string.h>
#include "../common/protocol.h"
#include "../common/mesh.h"

static int meal_is_safe(float temp_c, int approved_container_seen) {
    if (!approved_container_seen) {
        return 0;
    }
    return temp_c <= 5.0f;
}

int main(void) {
    float temp_c = 4.6f;
    int approved = 1;
    int safe = meal_is_safe(temp_c, approved);

    fas_mesh_config_t config = {.node_id = 0x0013, .slot_index = 3, .uplink_period_ms = 60000, .retries = 2};
    fas_mesh_init(&config);

    fas_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.preamble = FAS_PREAMBLE;
    frame.version = FAS_VERSION;
    frame.message_type = safe ? FAS_MSG_TELEMETRY : FAS_MSG_ALERT;
    frame.node_type = FAS_NODE_SAFE_LUNCH;
    frame.source_id = 0x0013;
    frame.destination_id = 0x0001;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, FAS_MAX_PAYLOAD, "temp=%.1f approved=%d", temp_c, approved);
    fas_mesh_send(&frame);

    printf("[safe-lunch] safe=%d temp=%.1f\n", safe, temp_c);
    return 0;
}
