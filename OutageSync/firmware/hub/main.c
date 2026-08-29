#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../common/mesh.h"
#include "../common/protocol.h"

typedef struct {
    uint32_t uptime_s;
    uint16_t active_nodes;
    uint16_t outage_events;
    bool lte_ready;
} hub_state_t;

static void queue_join_ack(os_mesh_queue_t *q, uint16_t node_id, uint16_t *seq) {
    os_frame_t frame;
    uint8_t payload[4] = {0x01, 0x00, 0x3C, 0x00};
    os_build_frame(&frame, OS_NODE_HUB, node_id, OS_MSG_JOIN_ACK, (*seq)++, 0x10203040u, payload, sizeof(payload));
    (void)os_mesh_enqueue(q, &frame, sizeof(payload));
}

static void print_recommendation(uint16_t reserve_minutes) {
    if (reserve_minutes > 240) {
        printf("policy=preserve-comfort generator=delay\n");
    } else if (reserve_minutes > 90) {
        printf("policy=shed-noncritical generator=standby\n");
    } else {
        printf("policy=charge-window generator=prepare\n");
    }
}

int main(void) {
    os_mesh_queue_t txq;
    os_mesh_init(&txq);

    hub_state_t state = {0};
    uint16_t seq = 1;
    queue_join_ack(&txq, OS_NODE_PANEL, &seq);
    queue_join_ack(&txq, (uint16_t)(OS_NODE_COLD_BASE + 1u), &seq);
    queue_join_ack(&txq, (uint16_t)(OS_NODE_OUTLET_BASE + 1u), &seq);
    queue_join_ack(&txq, OS_NODE_FUEL_SENTINEL, &seq);

    os_panel_status_t panel = {
        .vrms = 231,
        .irms_x10 = 183,
        .freq_x100 = 5997,
        .thd_x100 = 412,
        .grid_present = 0,
        .backup_mode = 1,
        .battery_soc_x10 = 524,
        .reserve_minutes = 132,
    };

    for (int minute = 0; minute < 4; ++minute) {
        state.uptime_s += 60u;
        state.active_nodes = 6u;
        state.outage_events = 1u;
        state.lte_ready = true;
        printf("hub uptime=%u nodes=%u reserve=%u soc=%.1f grid=%u lte=%u\n",
               state.uptime_s,
               state.active_nodes,
               panel.reserve_minutes,
               panel.battery_soc_x10 / 10.0,
               panel.grid_present,
               state.lte_ready ? 1u : 0u);
        print_recommendation(panel.reserve_minutes);
        panel.reserve_minutes = (uint16_t)(panel.reserve_minutes - 18u);
        panel.battery_soc_x10 = (uint16_t)(panel.battery_soc_x10 - 27u);
    }

    os_frame_t outbound;
    uint8_t payload_len = 0;
    while (os_mesh_dequeue(&txq, &outbound, &payload_len)) {
        printf("tx dst=%u type=%u len=%u crc=%04X\n",
               outbound.dst_id,
               outbound.msg_type,
               payload_len,
               outbound.crc);
    }
    return 0;
}
