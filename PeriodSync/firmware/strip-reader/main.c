#include <stdio.h>
#include "../common/mesh.h"
#include "../common/protocol.h"

int main(void) {
    mesh_config_t cfg = { .node_id = 0x2004, .channel = 3, .tx_power_dbm = 8 };
    mesh_init(&cfg);

    uint16_t normalized_intensity = 712;
    uint8_t payload[5] = { 1, (uint8_t)(normalized_intensity >> 8), (uint8_t)(normalized_intensity & 0xFF), 1, 7 };

    psync_frame_t frame;
    psync_build_frame(&frame, PSYNC_MSG_TELEMETRY, cfg.node_id, 0x1001, 1724390400UL, 0, payload, sizeof(payload));
    mesh_send_frame(&frame);
    printf("strip_reader assay=LH intensity=%u valid=1\n", normalized_intensity);
    return 0;
}
