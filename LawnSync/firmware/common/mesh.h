/*
 * LawnSync — TDMA Mesh Layer (Header)
 * Time-Division Multiple Access mesh networking for Sub-GHz
 */
#ifndef LAWNSYNC_MESH_H
#define LAWNSYNC_MESH_H

#include "protocol.h"
#include "sx1262.h"

#define LS_SLOT_COUNT      32    /* Total TDMA slots */
#define LS_SLOT_DURATION   50    /* ms per slot */
#define LS_FRAME_DURATION   (LS_SLOT_COUNT * LS_SLOT_DURATION) /* 1600 ms */
#define LS_HUB_NODE_ID     0x00
#define LS_MAX_RETRIES     3
#define LS_ACK_TIMEOUT_MS  2000

/* Node types */
typedef enum {
    LS_NODE_HUB = 0,
    LS_NODE_SOIL,
    LS_NODE_SPRINKLER,
    LS_NODE_WEATHER,
    LS_NODE_SCANNER,
} ls_node_type_t;

/* Mesh context */
typedef struct {
    uint8_t  node_id;        /* Assigned by hub */
    uint8_t  node_type;      /* ls_node_type_t */
    uint8_t  tdma_slot;      /* Assigned slot index */
    uint16_t msg_seq;        /* Message sequence counter */
    uint8_t  retry_count;    /* Current retry count */
    int8_t   last_rssi;      /* Last received RSSI */
    uint8_t  joined;         /* 1 if joined network */
    uint8_t  relay_enabled;  /* 1 if acting as relay */
    uint32_t last_hub_seen; /* Timestamp of last hub contact */
} ls_mesh_ctx_t;

/* Initialize mesh layer */
int ls_mesh_init(ls_mesh_ctx_t *ctx, uint8_t node_type,
                 const ls_spi_interface_t *spi,
                 const ls_radio_config_t *radio_cfg);

/* Join network (hub assigns node ID and slot) */
int ls_mesh_join(ls_mesh_ctx_t *ctx);

/* Send a message via mesh (with retries) */
int ls_mesh_send(ls_mesh_ctx_t *ctx, const ls_message_t *msg);

/* Receive a message (in assigned TDMA slot) */
int ls_mesh_recv(ls_mesh_ctx_t *ctx, ls_message_t *msg, uint32_t timeout_ms);

/* TDMA: wait for our assigned slot */
void ls_mesh_wait_slot(ls_mesh_ctx_t *ctx);

/* Relay a message (for out-of-range nodes) */
int ls_mesh_relay(ls_mesh_ctx_t *ctx, const ls_message_t *msg);

/* Hub: assign slot to joining node */
int ls_mesh_hub_assign_slot(ls_mesh_ctx_t *ctx, uint8_t node_type,
                            uint8_t *assigned_id, uint8_t *assigned_slot);

/* Hub: broadcast time sync */
int ls_mesh_hub_time_sync(ls_mesh_ctx_t *ctx, uint32_t epoch);

#endif /* LAWNSYNC_MESH_H */