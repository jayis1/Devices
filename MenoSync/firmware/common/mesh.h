/*
 * MenoSync — BLE + Sub-GHz Mesh Coordinator Helpers
 * Hub coordinator for managing connected nodes.
 */
#ifndef MENOSYNC_MESH_H
#define MENOSYNC_MESH_H

#include <stdint.h>
#include "protocol.h"

/* Maximum concurrent connections (Hub) */
#define MS_MESH_MAX_CONN   16

/* Node connection state */
typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;
    uint8_t  connected;
    uint8_t  battery_pct;
    uint8_t  rssi;
    uint8_t  link_type;       /* 0=BLE, 1=Sub-GHz */
    uint32_t last_seen_ms;
    uint16_t pkt_rx_count;
    uint16_t pkt_tx_count;
} ms_mesh_node_t;

/* Mesh context (used by Hub coordinator) */
typedef struct {
    ms_mesh_node_t nodes[MS_MESH_MAX_CONN];
    uint8_t        node_count;
    uint8_t        self_id;
    uint32_t       uptime_ms;
} ms_mesh_ctx_t;

/* Initialize mesh context */
void ms_mesh_init(ms_mesh_ctx_t *ctx, uint8_t self_id);

/* Register a newly connected node */
int ms_mesh_add_node(ms_mesh_ctx_t *ctx, uint8_t node_id, uint8_t node_type,
                     uint8_t link_type);

/* Remove a disconnected node */
int ms_mesh_remove_node(ms_mesh_ctx_t *ctx, uint8_t node_id);

/* Find a node by ID */
ms_mesh_node_t *ms_mesh_find_node(ms_mesh_ctx_t *ctx, uint8_t node_id);

/* Update node heartbeat / telemetry */
void ms_mesh_update_node(ms_mesh_ctx_t *ctx, uint8_t node_id,
                         uint8_t battery, uint8_t rssi);

/* Get count of connected nodes by type */
int ms_mesh_count_by_type(ms_mesh_ctx_t *ctx, uint8_t node_type);

/* Check if any nodes are overdue (heartbeat timeout) */
int ms_mesh_check_timeouts(ms_mesh_ctx_t *ctx, uint32_t now_ms, uint32_t timeout_s);

#endif /* MENOSYNC_MESH_H */