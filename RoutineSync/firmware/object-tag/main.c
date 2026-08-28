#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../common/mesh.h"
#include "../common/protocol.h"

static uint8_t compute_motion_state(uint16_t accel_mg) {
    if (accel_mg > 1800u) {
        return 2u;
    }
    if (accel_mg > 1050u) {
        return 1u;
    }
    return 0u;
}

int main(void) {
    rs_mesh_ctx_t mesh;
    rs_mesh_init(&mesh, RS_NODE_TAG_BASE + 1u, 0x7A6B5C4Du);

    rs_item_telemetry_t item;
    memset(&item, 0, sizeof(item));
    item.item_class = 3u; /* laptop sleeve */
    item.motion_state = compute_motion_state(1120u);
    item.battery_mv = 3975u;
    item.uwb_range_cm = 418u;
    item.room_anchor_id = 4u;
    item.critical_for_departure = 1u;
    item.last_seen_epoch = 1788032400u;

    rs_frame_t frame;
    rs_build_frame(&frame,
                   mesh.node_id,
                   RS_NODE_HUB,
                   RS_MSG_ITEM_TELEMETRY,
                   mesh.next_seq++,
                   mesh.session_nonce,
                   (const uint8_t *)&item,
                   (uint8_t)sizeof(item));

    printf("[tag] class=%u motion=%u battery=%umV range=%ucm room=%u crc=0x%04X\n",
           item.item_class,
           item.motion_state,
           item.battery_mv,
           item.uwb_range_cm,
           item.room_anchor_id,
           frame.crc);
    return 0;
}
