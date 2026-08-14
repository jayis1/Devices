#include "mesh.h"
#include <string.h>

void mesh_init(mesh_state_t *state, uint16_t node_id, bool coordinator)
{
    memset(state, 0, sizeof(*state));
    state->my_node_id = node_id;
    state->is_coordinator = coordinator;
    state->my_slot = coordinator ? 0 : 0xFF;
    state->num_active_nodes = 0;
    state->tx_seq = 0;
    state->superframe_start = 0;

    for (int i = 0; i < MESH_MAX_NODES; i++) {
        state->nodes[i].active = false;
    }
}

bool mesh_add_node(mesh_state_t *state, uint16_t node_id, uint8_t type)
{
    /* Check if already exists */
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (state->nodes[i].active && state->nodes[i].node_id == node_id) {
            return true;
        }
    }
    /* Find free slot */
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (!state->nodes[i].active) {
            state->nodes[i].node_id = node_id;
            state->nodes[i].node_type = type;
            state->nodes[i].slot = mesh_assign_slot(state, node_id);
            state->nodes[i].rssi = 0;
            state->nodes[i].last_seen = 0;
            state->nodes[i].active = true;
            state->num_active_nodes++;
            return true;
        }
    }
    return false;
}

bool mesh_remove_node(mesh_state_t *state, uint16_t node_id)
{
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (state->nodes[i].active && state->nodes[i].node_id == node_id) {
            state->nodes[i].active = false;
            state->num_active_nodes--;
            return true;
        }
    }
    return false;
}

uint8_t mesh_assign_slot(mesh_state_t *state, uint16_t node_id)
{
    /* Slot 0 is coordinator/beacon, slots 1+ for nodes */
    bool used[TDMA_MAX_SLOTS] = {false};
    used[0] = true; /* beacon */

    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (state->nodes[i].active && state->nodes[i].slot < TDMA_MAX_SLOTS) {
            used[state->nodes[i].slot] = true;
        }
    }

    for (int s = 1; s < TDMA_MAX_SLOTS; s++) {
        if (!used[s]) return (uint8_t)s;
    }
    return 0xFF; /* no slot available */
}

void mesh_update_node(mesh_state_t *state, uint16_t node_id, int8_t rssi)
{
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (state->nodes[i].active && state->nodes[i].node_id == node_id) {
            state->nodes[i].rssi = rssi;
            state->nodes[i].last_seen = 0; /* updated by caller with timestamp */
            return;
        }
    }
}

void mesh_tick(mesh_state_t *state, uint32_t now_ms)
{
    /* Check for stale nodes (missed 3 superframes = 3 seconds) */
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (state->nodes[i].active) {
            if (now_ms - state->nodes[i].last_seen > 30000) {
                mesh_remove_node(state, state->nodes[i].node_id);
            }
        }
    }
}

bool mesh_is_my_slot(mesh_state_t *state, uint32_t now_ms)
{
    if (state->my_slot == 0xFF) return false;

    uint32_t elapsed = now_ms - state->superframe_start;
    uint32_t slot_start = TDMA_BEACON_MS + (state->my_slot - 1) * TDMA_SLOT_MS;
    uint32_t slot_end = slot_start + TDMA_SLOT_MS;

    return (elapsed >= slot_start && elapsed < slot_end);
}

uint32_t mesh_time_to_next_slot(mesh_state_t *state, uint32_t now_ms)
{
    if (state->my_slot == 0xFF) return 0xFFFFFFFF;

    uint32_t elapsed = now_ms - state->superframe_start;
    uint32_t slot_start = TDMA_BEACON_MS + (state->my_slot - 1) * TDMA_SLOT_MS;

    if (elapsed < slot_start) {
        return slot_start - elapsed;
    }
    /* Next superframe */
    return TDMA_SUPERFRAME_MS - elapsed + slot_start;
}

void mesh_build_beacon(mesh_state_t *state, beacon_t *beacon, uint32_t timestamp)
{
    beacon->timestamp = timestamp;
    beacon->num_slots = TDMA_MAX_SLOTS;
    memset(beacon->slot_assignments, 0, TDMA_MAX_SLOTS);

    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (state->nodes[i].active && state->nodes[i].slot < TDMA_MAX_SLOTS) {
            beacon->slot_assignments[state->nodes[i].slot] = (uint8_t)(state->nodes[i].node_id & 0xFF);
        }
    }
    beacon->channel = 0;
    beacon->rssi = 0;
}