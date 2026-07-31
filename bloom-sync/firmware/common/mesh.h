/*
 * BloomSync — BLE WAN Mesh/Coordinator Helpers
 * Central coordinator for managing connected nodes via BLE 5.0.
 */
#ifndef BLOOMSYNC_MESH_H
#define BLOOMSYNC_MESH_H

#include <stdint.h>
#include "protocol.h"

/* Maximum concurrent BLE connections (Hub) */
#define BS_MESH_MAX_CONN   7

/* Node connection state */
typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;
    uint8_t  connected;
    uint8_t  battery_pct;
    uint8_t  rssi;
    uint32_t last_seen_ms;
    uint16_t pkt_rx_count;
    uint16_t pkt_tx_count;
} bs_mesh_node_t;

/* Mesh context (used by Hub coordinator) */
typedef struct {
    bs_mesh_node_t nodes[BS_MESH_MAX_CONN];
    uint8_t        node_count;
    uint8_t        self_id;
    uint32_t       uptime_ms;
} bs_mesh_ctx_t;

/* Initialize mesh context */
void bs_mesh_init(bs_mesh_ctx_t *ctx, uint8_t self_id);

/* Register a newly connected node */
int bs_mesh_add_node(bs_mesh_ctx_t *ctx, uint8_t node_id, uint8_t node_type);

/* Remove a disconnected node */
int bs_mesh_remove_node(bs_mesh_ctx_t *ctx, uint8_t node_id);

/* Find a node by ID */
bs_mesh_node_t *bs_mesh_find_node(bs_mesh_ctx_t *ctx, uint8_t node_id);

/* Update node heartbeat / telemetry */
void bs_mesh_update_node(bs_mesh_ctx_t *ctx, uint8_t node_id,
                         uint8_t battery, uint8_t rssi);

/* Get count of connected nodes by type */
int bs_mesh_count_by_type(bs_mesh_ctx_t *ctx, uint8_t node_type);

/* Check if any nodes are overdue (heartbeat timeout) */
int bs_mesh_check_timeouts(bs_mesh_ctx_t *ctx, uint32_t now_ms, uint32_t timeout_s);

#endif /* BLOOMSYNC_MESH_H */