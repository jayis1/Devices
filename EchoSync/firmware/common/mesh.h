/*
 * EchoSync — TDMA Mesh Layer (Header)
 * Time-Division Multiple Access mesh networking for Sub-GHz
 */
#ifndef ECHOSYNC_MESH_H
#define ECHOSYNC_MESH_H

#include "protocol.h"
#include "sx1262.h"

#define ES_SLOT_COUNT      20    /* Total TDMA slots (16 nodes + relay) */
#define ES_SLOT_DURATION   50    /* ms per slot */
#define ES_FRAME_DURATION  (ES_SLOT_COUNT * ES_SLOT_DURATION) /* 1000 ms */
#define ES_HUB_NODE_ID     0x00
#define ES_MAX_RETRIES     3
#define ES_ACK_TIMEOUT_MS  2000

/* Node types */
typedef enum {
    ES_NODE_HUB = 0,
    ES_NODE_SENTINEL,
    ES_NODE_WRIST,
    ES_NODE_DOOR,
} es_node_type_t;

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
    uint8_t  emergency_mode;
} es_mesh_ctx_t;

/* Initialize mesh layer */
int es_mesh_init(es_mesh_ctx_t *ctx, uint8_t node_type,
                 const es_spi_interface_t *spi,
                 const es_radio_config_t *radio_cfg);

/* Join network (hub assigns node ID and slot) */
int es_mesh_join(es_mesh_ctx_t *ctx);

/* Send a message via mesh (with retries) */
int es_mesh_send(es_mesh_ctx_t *ctx, const es_message_t *msg);

/* Receive a message (in assigned TDMA slot) */
int es_mesh_recv(es_mesh_ctx_t *ctx, es_message_t *msg, uint32_t timeout_ms);

/* TDMA: wait for our assigned slot */
void es_mesh_wait_slot(es_mesh_ctx_t *ctx);

/* Relay a message (for out-of-range nodes) */
int es_mesh_relay(es_mesh_ctx_t *ctx, const es_message_t *msg);

/* Hub: assign slot to joining node */
int es_mesh_hub_assign_slot(es_mesh_ctx_t *ctx, uint8_t node_type,
                            uint8_t *assigned_id, uint8_t *assigned_slot);

/* Hub: broadcast time sync */
int es_mesh_hub_time_sync(es_mesh_ctx_t *ctx, uint32_t epoch);

#endif /* ECHOSYNC_MESH_H */