#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../common/mesh.h"
#include "../common/protocol.h"

typedef struct {
    uint32_t uptime_s;
    uint32_t telemetry_published;
    uint8_t online_nodes;
    bool internet_up;
} hub_state_t;

static void publish_status(const hub_state_t *state) {
    printf("hub uptime=%u nodes=%u mqtt=%u published=%u\n",
           state->uptime_s,
           state->online_nodes,
           state->internet_up ? 1u : 0u,
           state->telemetry_published);
}

static void schedule_join_ack(ws_mesh_queue_t *queue, uint16_t node_id, uint16_t *seq) {
    ws_frame_t frame;
    uint8_t payload[4] = {0x01, 0x00, 0x30, 0x00};
    ws_build_frame(&frame, WS_NODE_HUB, node_id, WS_MSG_JOIN_ACK, (*seq)++, 0xABCDEF01u, payload, sizeof(payload));
    (void)ws_mesh_enqueue(queue, &frame, sizeof(payload));
}

static void handle_sort_event(const ws_sort_event_t *event) {
    const char *stream = "unknown";
    if (event->recommended_stream == WS_STREAM_RECYCLE) stream = "recycle";
    else if (event->recommended_stream == WS_STREAM_COMPOST) stream = "compost";
    else if (event->recommended_stream == WS_STREAM_LANDFILL) stream = "landfill";
    printf("sort item=%u stream=%s confidence=%u contamination=%u%%\n",
           event->item_id, stream, event->confidence_q15, event->contamination_risk_pct);
}

int main(void) {
    ws_mesh_queue_t txq;
    ws_mesh_init(&txq);

    hub_state_t state = {0};
    uint16_t seq = 1;
    schedule_join_ack(&txq, WS_NODE_SORTER, &seq);
    schedule_join_ack(&txq, (uint16_t)(WS_NODE_BIN_DOCK_BASE + 1u), &seq);
    schedule_join_ack(&txq, WS_NODE_PICKUP_BEACON, &seq);

    ws_sort_event_t sample = {
        .item_id = 42,
        .recommended_stream = WS_STREAM_RECYCLE,
        .material_class = 3,
        .confidence_q15 = 30211,
        .contamination_risk_pct = 14,
        .barcode_present = 1,
        .mass_grams = 18,
        .reserved = 0,
    };
    handle_sort_event(&sample);

    for (int tick = 0; tick < 5; ++tick) {
        state.uptime_s += 60u;
        state.online_nodes = 4u;
        state.internet_up = true;
        state.telemetry_published += 3u;
        publish_status(&state);
    }

    ws_frame_t outbound;
    uint8_t payload_len = 0;
    while (ws_mesh_dequeue(&txq, &outbound, &payload_len)) {
        printf("tx frame dst=%u type=%u payload=%u crc=%04X\n",
               outbound.dst_id,
               outbound.msg_type,
               payload_len,
               outbound.crc);
    }

    return 0;
}
