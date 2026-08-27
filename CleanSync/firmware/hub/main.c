#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../common/mesh.h"
#include "../common/protocol.h"

typedef struct {
    uint32_t uptime_s;
    uint8_t online_nodes;
    uint16_t missions_dispatched;
    bool lte_backup_ready;
} hub_state_t;

static void publish_status(const hub_state_t *state) {
    printf("hub uptime=%u online=%u missions=%u lte=%u\n",
           state->uptime_s,
           state->online_nodes,
           state->missions_dispatched,
           state->lte_backup_ready ? 1u : 0u);
}

static void queue_join_ack(cs_mesh_queue_t *queue, uint16_t node_id, uint16_t *seq) {
    cs_frame_t frame;
    const uint8_t payload[4] = {0x01, 0x00, 0x20, 0x00};
    cs_build_frame(&frame, CS_NODE_HUB, node_id, CS_MSG_JOIN_ACK, (*seq)++, 0xC1EA5001u, payload, sizeof(payload));
    (void)cs_mesh_enqueue(queue, &frame, (uint8_t)sizeof(payload));
}

int main(void) {
    cs_mesh_queue_t txq;
    cs_mesh_init(&txq);

    hub_state_t state = {0};
    uint16_t seq = 1u;
    queue_join_ack(&txq, (uint16_t)(CS_NODE_DIRT_BASE + 1u), &seq);
    queue_join_ack(&txq, CS_NODE_DOCK, &seq);
    queue_join_ack(&txq, CS_NODE_WAND, &seq);

    for (int i = 0; i < 5; ++i) {
        state.uptime_s += 60u;
        state.online_nodes = 4u;
        state.missions_dispatched += 1u;
        state.lte_backup_ready = true;
        publish_status(&state);
    }

    cs_frame_t frame;
    uint8_t length = 0u;
    while (cs_mesh_dequeue(&txq, &frame, &length)) {
        printf("tx dst=%u type=%u payload=%u crc=%04X\n", frame.dst_id, frame.msg_type, length, frame.crc);
    }

    return 0;
}
