/*
 * GrillSync — TDMA Mesh Networking Layer Header
 */
#ifndef GRILLSYNC_MESH_H
#define GRILLSYNC_MESH_H

#include <stdint.h>
#include "protocol.h"
#include "sx1262.h"

/* Node types */
enum gs_node_type {
    GS_NODE_HUB         = 0,
    GS_NODE_SENTINEL    = 1,
    GS_NODE_PROBE       = 2,
    GS_NODE_SMOKE       = 3,
};

/* Mesh context */
typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;
    uint8_t  tdma_slot;
    uint8_t  joined;
    uint16_t msg_seq;
    uint32_t last_time_sync;
    /* Radio */
    gs_radio_ctx_t radio;
    /* For hub: node table */
    uint8_t  slot_assigned[GS_SLOT_COUNT];
    uint8_t  slot_node_type[GS_SLOT_COUNT];
} gs_mesh_ctx_t;

/* API */
int gs_mesh_init(gs_mesh_ctx_t *mesh, uint8_t node_type,
                 const gs_spi_interface_t *spi,
                 const gs_radio_config_t *radio_cfg);
int gs_mesh_send(gs_mesh_ctx_t *mesh, const gs_message_t *msg);
int gs_mesh_recv(gs_mesh_ctx_t *mesh, gs_message_t *msg, uint32_t timeout_ms);
int gs_mesh_hub_assign_slot(gs_mesh_ctx_t *mesh, uint8_t node_type,
                             uint8_t *node_id, uint8_t *slot);
void gs_mesh_hub_time_sync(gs_mesh_ctx_t *mesh, uint32_t epoch);
int gs_mesh_join(gs_mesh_ctx_t *mesh, uint8_t node_type, uint8_t battery_mv,
                 uint8_t fw_version);
int gs_mesh_heartbeat(gs_mesh_ctx_t *mesh, uint8_t battery_mv, int8_t rssi);

#endif /* GRILLSYNC_MESH_H */