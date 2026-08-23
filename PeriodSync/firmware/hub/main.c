#include <stdio.h>
#include <string.h>
#include "../common/mesh.h"
#include "../common/protocol.h"

static void publish_local_dashboard(float phase_confidence, float leak_risk) {
    printf("dashboard phase=%.2f leak=%.2f\n", phase_confidence, leak_risk);
}

int main(void) {
    mesh_config_t cfg = { .node_id = 0x1001, .channel = 3, .tx_power_dbm = 14 };
    mesh_init(&cfg);

    psync_frame_t frame;
    uint8_t payload[8] = { 78, 64, 0, 0, 0, 0, 0, 0 };
    psync_build_frame(&frame, PSYNC_MSG_TELEMETRY, cfg.node_id, 0x0000, 1724390400UL, 0, payload, sizeof(payload));
    mesh_send_frame(&frame);
    publish_local_dashboard(0.78f, 0.18f);

    while (1) {
        if (mesh_receive_frame(&frame)) {
            printf("rx type=%u from=%u\n", frame.message_type, frame.source_node_id);
        }
        break;
    }
    return 0;
}
