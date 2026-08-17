/*
 * TremorSync Sub-GHz TDMA mesh
 * Coordinator (Hub) assigns slots; nodes sleep between their slot and beacon.
 */
#include "mesh.h"
#include <string.h>

void mesh_init(mesh_state_t *state, uint16_t self_id, bool coordinator)
{
    memset(state, 0, sizeof(*state));
    state->self_id        = self_id;
    state->is_coordinator = coordinator;
    state->node_count     = 0;
    state->seq_counter    = 0;
    state->superframe_start_ms = 0;
}

int mesh_find_node(mesh_state_t *state, uint16_t node_id)
{
    for (int i = 0; i < state->node_count; i++) {
        if (state->nodes[i].node_id == node_id) return i;
    }
    return -1;
}

int mesh_add_node(mesh_state_t *state, uint16_t node_id)
{
    if (mesh_find_node(state, node_id) >= 0) return -1;  /* already exists */
    if (state->node_count >= MESH_MAX_NODES) return -1;

    int idx = state->node_count++;
    state->nodes[idx].node_id = node_id;
    state->nodes[idx].slot    = mesh_assign_slot(state);
    state->nodes[idx].active  = true;
    state->nodes[idx].rssi    = 0;
    return idx;
}

uint8_t mesh_assign_slot(mesh_state_t *state)
{
    /* Slots 0..1 are for the first two non-coordinator nodes; rest reserved. */
    return (uint8_t)(state->node_count + 1);
}

void mesh_remove_stale(mesh_state_t *state, uint32_t now_ms)
{
    for (int i = state->node_count - 1; i >= 0; i--) {
        if (now_ms - state->nodes[i].last_seen_ms > 60000) {  /* 60 s timeout */
            state->nodes[i] = state->nodes[state->node_count - 1];
            state->node_count--;
        }
    }
}

void mesh_build_beacon(mesh_state_t *state, mesh_frame_t *frame)
{
    /* Beacon payload: slot assignments for up to 8 nodes (compact) */
    uint8_t payload[FRAME_PAYLOAD_MAX];
    memset(payload, 0, FRAME_PAYLOAD_MAX);

    payload[0] = state->node_count;
    for (int i = 0; i < state->node_count && i < 8; i++) {
        payload[1 + i * 2]     = (state->nodes[i].node_id >> 8) & 0xFF;
        payload[1 + i * 2 + 1] = state->nodes[i].node_id & 0xFF;
        payload[17 + i]        = state->nodes[i].slot;
    }

    protocol_build_frame(frame, state->self_id, NODE_ID_BROADCAST,
                         MSG_TYPE_BEACON, state->seq_counter++,
                         payload, FRAME_PAYLOAD_MAX);
}

bool mesh_process_frame(mesh_state_t *state, const mesh_frame_t *frame,
                         uint32_t now_ms)
{
    if (frame->dst_id != state->self_id &&
        frame->dst_id != NODE_ID_BROADCAST) {
        return false;
    }

    switch (frame->msg_type) {
    case MSG_TYPE_JOIN_REQ: {
        mesh_add_node(state, frame->src_id);
        return true;
    }
    case MSG_TYPE_HEARTBEAT:
    case MSG_TYPE_SENSOR_DATA: {
        int idx = mesh_find_node(state, frame->src_id);
        if (idx >= 0) {
            state->nodes[idx].last_seen_ms = now_ms;
            state->nodes[idx].active = true;
        } else {
            mesh_add_node(state, frame->src_id);
        }
        return true;
    }
    default:
        return true;
    }
}