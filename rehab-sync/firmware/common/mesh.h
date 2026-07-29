/*
 * RehabSync — TDMA Mesh Network Layer Header
 */
#ifndef REHABSYNC_MESH_H
#define REHABSYNC_MESH_H

#include <stdint.h>
#include <stdbool.h>
#include "sx1262.h"
#include "protocol.h"

#define RS_SLOT_COUNT       16
#define RS_SLOT_DURATION_MS 125  /* 16 slots × 125 ms = 2 s TDMA frame */

/* Node table entry */
typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;
    uint8_t  slot;
    int8_t   rssi;
    uint32_t last_seen;
    bool     active;
} rs_node_entry_t;

/* Mesh context */
typedef struct {
    uint8_t           self_id;
    uint8_t           self_type;
    bool              is_coordinator;
    sx1262_t         *radio;
    rs_node_entry_t   node_table[RS_SLOT_COUNT];
    uint8_t           my_slot;
    uint32_t          frame_counter;
    uint8_t           seq_counter;
    bool              joined;
} rs_mesh_ctx_t;

/* API */
int  rs_mesh_init(rs_mesh_ctx_t *mesh, uint8_t self_id, uint8_t self_type,
                  bool is_coordinator, sx1262_t *radio);
int  rs_mesh_join(rs_mesh_ctx_t *mesh, uint32_t timeout_ms);
int  rs_mesh_send(rs_mesh_ctx_t *mesh, uint8_t dst_id,
                  uint8_t msg_type, uint8_t subtype,
                  const uint8_t *payload, size_t len);
int  rs_mesh_recv(rs_mesh_ctx_t *mesh, rs_msg_header_t *hdr,
                  uint8_t *payload, size_t cap, uint32_t timeout_ms);
int  rs_mesh_broadcast(rs_mesh_ctx_t *mesh, uint8_t msg_type, uint8_t subtype,
                       const uint8_t *payload, size_t len);
void rs_mesh_heartbeat(rs_mesh_ctx_t *mesh);
int  rs_mesh_assign_slot(rs_mesh_ctx_t *mesh, uint8_t node_id, uint8_t node_type);
void rs_mesh_prune_stale(rs_mesh_ctx_t *mesh, uint32_t max_age_s);

#endif /* REHABSYNC_MESH_H */