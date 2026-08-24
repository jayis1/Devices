#include <stdio.h>
#include <string.h>
#include "../common/protocol.h"
#include "../common/mesh.h"

static int injector_ready(int present, float temperature_c, int days_to_expiry) {
    return present && temperature_c > 2.0f && temperature_c < 30.0f && days_to_expiry > 14;
}

int main(void) {
    int present = 1;
    float temperature_c = 24.0f;
    int days_to_expiry = 180;
    int ready = injector_ready(present, temperature_c, days_to_expiry);

    fas_mesh_config_t config = {.node_id = 0x0014, .slot_index = 4, .uplink_period_ms = 30000, .retries = 3};
    fas_mesh_init(&config);

    fas_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.preamble = FAS_PREAMBLE;
    frame.version = FAS_VERSION;
    frame.message_type = ready ? FAS_MSG_TELEMETRY : FAS_MSG_ALERT;
    frame.node_type = FAS_NODE_EPIPEN_GUARD;
    frame.source_id = 0x0014;
    frame.destination_id = 0x0001;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, FAS_MAX_PAYLOAD, "ready=%d days=%d", ready, days_to_expiry);
    fas_mesh_send(&frame);

    printf("[epipen-guard] ready=%d\n", ready);
    return 0;
}
