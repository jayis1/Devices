#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"
#include "../common/protocol.h"

static uint16_t estimate_tray_mass_delta_g(const uint16_t *channels, size_t count, uint16_t expected_g) {
    uint32_t sum = 0u;
    for (size_t i = 0; i < count; ++i) {
        sum += channels[i];
    }
    uint16_t measured = (uint16_t)(sum / (count > 0 ? count : 1));
    return (measured > expected_g) ? (uint16_t)(measured - expected_g) : (uint16_t)(expected_g - measured);
}

int main(void) {
    rs_mesh_ctx_t mesh;
    rs_mesh_init(&mesh, RS_NODE_DOORWAY_DOCK, 0x0DD0C0DEu);

    uint16_t channels[4] = {520u, 512u, 527u, 519u};
    rs_doorway_status_t status;
    memset(&status, 0, sizeof(status));
    status.routine_id = 2;
    status.missing_items = 1;
    status.checklist_complete_pct = 75;
    status.door_open = 1;
    status.tray_mass_delta_g = estimate_tray_mass_delta_g(channels, 4u, 380u);
    status.nfc_flags = 0x0001;
    status.nearest_tag_range_cm = 287u;
    status.minutes_to_deadline = 8u;

    rs_frame_t frame;
    rs_build_frame(&frame,
                   mesh.node_id,
                   RS_NODE_HUB,
                   RS_MSG_DOORWAY_STATUS,
                   mesh.next_seq++,
                   mesh.session_nonce,
                   (const uint8_t *)&status,
                   (uint8_t)sizeof(status));

    printf("[doorway] missing=%u checklist=%u%% tray_delta=%ug range=%ucm crc=0x%04X\n",
           status.missing_items,
           status.checklist_complete_pct,
           status.tray_mass_delta_g,
           status.nearest_tag_range_cm,
           frame.crc);
    return 0;
}
