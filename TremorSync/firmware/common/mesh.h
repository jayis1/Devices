#ifndef TREMORSYNC_MESH_H
#define TREMORSYNC_MESH_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

/* TDMA superframe timing (milliseconds) */
#define TDMA_SUPERFRAME_MS    1000
#define TDMA_BEACON_MS        20
#define TDMA_SLOT_MS          50
#define TDMAX_SLOTS           19   /* 2 active + 9 reserved + 8 relay */

/* Mesh node state */
typedef struct {
    uint16_t node_id;
    uint8_t  slot;
    bool     active;
    uint32_t last_seen_ms;
    uint8_t  rssi;
} mesh_node_entry_t;

#define MESH_MAX_NODES 16

typedef struct {
    uint16_t        self_id;
    bool            is_coordinator;
    uint32_t        superframe_start_ms;
    mesh_node_entry_t nodes[MESH_MAX_NODES];
    uint8_t         node_count;
    uint16_t        seq_counter;
} mesh_state_t;

/* API */
void mesh_init(mesh_state_t *state, uint16_t self_id, bool coordinator);
int  mesh_add_node(mesh_state_t *state, uint16_t node_id);
int  mesh_find_node(mesh_state_t *state, uint16_t node_id);
void mesh_remove_stale(mesh_state_t *state, uint32_t now_ms);
uint8_t mesh_assign_slot(mesh_state_t *state);
void mesh_build_beacon(mesh_state_t *state, mesh_frame_t *frame);
bool mesh_process_frame(mesh_state_t *state, const mesh_frame_t *frame,
                         uint32_t now_ms);

#endif /* TREMORSYNC_MESH_H */