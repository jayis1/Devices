#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../common/mesh.h"
#include "../common/protocol.h"

typedef struct {
    uint32_t uptime_s;
    uint8_t readiness_score;
    uint8_t lateness_risk_pct;
    uint8_t online_nodes;
} hub_state_t;

static void publish_overview(const hub_state_t *state) {
    printf("hub uptime=%u readiness=%u lateness=%u nodes=%u\n",
           state->uptime_s,
           state->readiness_score,
           state->lateness_risk_pct,
           state->online_nodes);
}

static void queue_join_ack(ss_mesh_queue_t *queue, uint16_t node_id, uint16_t *seq) {
    uint8_t payload[4] = {0x01, 0x00, 0x3C, 0x00};
    ss_frame_t frame;
    ss_build_frame(&frame, SS_NODE_HUB, node_id, SS_MSG_JOIN_ACK, (*seq)++, 0x5151A1A1u, payload, sizeof(payload));
    (void)ss_mesh_enqueue(queue, &frame, sizeof(payload));
}

int main(void) {
    ss_mesh_queue_t queue;
    ss_mesh_init(&queue);
    uint16_t seq = 1;
    queue_join_ack(&queue, SS_NODE_BACKPACK, &seq);
    queue_join_ack(&queue, SS_NODE_LUNCH_DOCK, &seq);
    queue_join_ack(&queue, SS_NODE_DOORWAY, &seq);
    queue_join_ack(&queue, SS_NODE_TRANSIT, &seq);

    hub_state_t state = {0, 82, 18, 5};
    for (int i = 0; i < 5; ++i) {
        state.uptime_s += 60u;
        state.readiness_score = (uint8_t)(82 - i * 3);
        state.lateness_risk_pct = (uint8_t)(18 + i * 7);
        publish_overview(&state);
    }

    ss_frame_t frame;
    uint8_t len = 0;
    while (ss_mesh_dequeue(&queue, &frame, &len)) {
        printf("tx dst=%u type=%u len=%u crc=%04X\n", frame.dst_id, frame.msg_type, len, frame.crc);
    }
    return 0;
}
