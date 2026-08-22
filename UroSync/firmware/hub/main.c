#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../common/mesh.h"
#include "../common/protocol.h"

typedef struct {
    uint32_t uptime_s;
    uint8_t online_nodes;
    uint16_t hydration_risk_pct;
    uint16_t uti_risk_pct;
    bool leak_alarm;
} hub_state_t;

static void publish_status(const hub_state_t *state) {
    printf("hub uptime=%u nodes=%u hydration=%u uti=%u leak=%u\n",
           state->uptime_s,
           state->online_nodes,
           state->hydration_risk_pct,
           state->uti_risk_pct,
           state->leak_alarm ? 1u : 0u);
}

static void queue_join_ack(us_mesh_queue_t *queue, uint16_t node_id, uint16_t *seq) {
    us_frame_t frame;
    uint8_t payload[4] = {0x01, 0x00, 0x2A, 0x00};
    us_build_frame(&frame, US_NODE_HUB, node_id, US_MSG_JOIN_ACK, (*seq)++, 0x13572468u, payload, sizeof(payload));
    (void)us_mesh_enqueue(queue, &frame, sizeof(payload));
}

static void ingest_void_event(const us_void_event_t *event, hub_state_t *state) {
    state->hydration_risk_pct = (uint16_t)((event->sg_q1000 > 1020 ? 70 : 32) + (event->color_index > 6 ? 15 : 0));
    state->uti_risk_pct = (uint16_t)((event->leukocyte ? 40 : 5) + (event->nitrite ? 35 : 0) + (event->blood ? 10 : 0));
    printf("void volume_ml=%u peak_flow=%u color=%u sg=%u uti=%u\n",
           event->volume_ml,
           event->peak_flow_ml_min,
           event->color_index,
           event->sg_q1000,
           state->uti_risk_pct);
}

int main(void) {
    us_mesh_queue_t txq;
    us_mesh_init(&txq);

    uint16_t seq = 1;
    queue_join_ack(&txq, US_NODE_TOILET_DOCK, &seq);
    queue_join_ack(&txq, US_NODE_NIGHT_MAT, &seq);
    queue_join_ack(&txq, US_NODE_BOTTLE_TAG, &seq);
    queue_join_ack(&txq, US_NODE_BATH_SENTINEL, &seq);

    hub_state_t state = {0};
    us_void_event_t sample = {
        .event_id = 7001,
        .volume_ml = 420,
        .peak_flow_ml_min = 168,
        .flow_duration_s = 22,
        .color_index = 7,
        .leukocyte = 1,
        .nitrite = 0,
        .blood = 0,
        .protein = 1,
        .ketone = 0,
        .glucose = 0,
        .ph_bin = 6,
        .sg_q1000 = 1024,
    };
    ingest_void_event(&sample, &state);

    for (int tick = 0; tick < 4; ++tick) {
        state.uptime_s += 60u;
        state.online_nodes = 5u;
        publish_status(&state);
    }

    us_frame_t frame;
    uint8_t payload_len = 0;
    while (us_mesh_dequeue(&txq, &frame, &payload_len)) {
        printf("tx dst=%u type=%u payload=%u crc=%04X\n", frame.dst_id, frame.msg_type, payload_len, frame.crc);
    }
    return 0;
}
