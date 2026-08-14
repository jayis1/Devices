#ifndef POSTURESYNC_MESH_H
#define POSTURESYNC_MESH_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

/* Mesh node info */
typedef struct {
    uint16_t node_id;
    uint8_t  node_type;
    uint8_t  slot;
    int8_t   rssi;
    uint32_t last_seen;
    bool     active;
} mesh_node_t;

/* Mesh state */
#define MESH_MAX_NODES  16

typedef struct {
    uint16_t my_node_id;
    bool     is_coordinator;
    uint32_t superframe_start;
    uint8_t  my_slot;
    uint8_t  num_active_nodes;
    mesh_node_t nodes[MESH_MAX_NODES];
    uint16_t tx_seq;
} mesh_state_t;

/* API */
void mesh_init(mesh_state_t *state, uint16_t node_id, bool coordinator);
bool mesh_add_node(mesh_state_t *state, uint16_t node_id, uint8_t type);
bool mesh_remove_node(mesh_state_t *state, uint16_t node_id);
uint8_t mesh_assign_slot(mesh_state_t *state, uint16_t node_id);
void mesh_update_node(mesh_state_t *state, uint16_t node_id, int8_t rssi);
void mesh_tick(mesh_state_t *state, uint32_t now_ms);
bool mesh_is_my_slot(mesh_state_t *state, uint32_t now_ms);
uint32_t mesh_time_to_next_slot(mesh_state_t *state, uint32_t now_ms);

/* Beacon builder */
void mesh_build_bacon(mesh_state_t *state, beacon_t *beacon, uint32_t timestamp);

#endif /* POSTURESYNC_MESH_H */