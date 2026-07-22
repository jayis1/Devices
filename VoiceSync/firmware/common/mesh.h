/*
 * VoiceSync — TDMA Mesh Layer (Header)
 * Time-Division Multiple Access mesh networking for Sub-GHz
 */
#ifndef VOICESYNC_MESH_H
#define VOICESYNC_MESH_H

#include "protocol.h"
#include "sx1262.h"

#define VS_SLOT_COUNT      20    /* Total TDMA slots (16 nodes + relay) */
#define VS_SLOT_DURATION   50    /* ms per slot */
#define VS_FRAME_DURATION  (VS_SLOT_COUNT * VS_SLOT_DURATION) /* 1000 ms */
#define VS_HUB_NODE_ID     0x00
#define VS_MAX_RETRIES     3
#define VS_ACK_TIMEOUT_MS  2000

/* Node types */
typedef enum {
    VS_NODE_HUB = 0,
    VS_NODE_VOCAL_BAND,
    VS_NODE_ROOM,
    VS_NODE_HYDRATION,
    VS_NODE_HUMIDITY,
} vs_node_type_t;

/* Mesh context */
typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;
    uint8_t  tdma_slot;
    uint16_t msg_seq;
    uint8_t  retry_count;
    int8_t   last_rssi;
    uint8_t  joined;
    uint8_t  relay_enabled;
    uint32_t last_hub_seen;
    uint8_t  high_risk_mode;
} vs_mesh_ctx_t;

/* Initialize mesh layer */
int vs_mesh_init(vs_mesh_ctx_t *ctx, uint8_t node_type,
                 const vs_spi_interface_t *spi,
                 const vs_radio_config_t *radio_cfg);

/* Join network (hub assigns node ID and slot) */
int vs_mesh_join(vs_mesh_ctx_t *ctx);

/* Send a message via mesh (with retries) */
int vs_mesh_send(vs_mesh_ctx_t *ctx, const vs_message_t *msg);

/* Receive a message (in assigned TDMA slot) */
int vs_mesh_recv(vs_mesh_ctx_t *ctx, vs_message_t *msg, uint32_t timeout_ms);

/* TDMA: wait for our assigned slot */
void vs_mesh_wait_slot(vs_mesh_ctx_t *ctx);

/* Relay a message (for out-of-range nodes) */
int vs_mesh_relay(vs_mesh_ctx_t *ctx, const vs_message_t *msg);

/* Hub: assign slot to joining node */
int vs_mesh_hub_assign_slot(vs_mesh_ctx_t *ctx, uint8_t node_type,
                            uint8_t *assigned_id, uint8_t *assigned_slot);

/* Hub: broadcast time sync */
int vs_mesh_hub_time_sync(vs_mesh_ctx_t *ctx, uint32_t epoch);

#endif /* VOICESYNC_MESH_H */