/*
 * MosquitoSync — TDMA Mesh Layer (Header)
 * Time-Division Multiple Access mesh networking for Sub-GHz
 */
#ifndef MOSQUITOSYNC_MESH_H
#define MOSQUITOSYNC_MESH_H

#include "protocol.h"
#include "sx1262.h"

#define MS_SLOT_COUNT      30    /* Total TDMA slots (24 nodes + relay slots) */
#define MS_SLOT_DURATION   50    /* ms per slot */
#define MS_FRAME_DURATION  (MS_SLOT_COUNT * MS_SLOT_DURATION) /* 1500 ms */
#define MS_HUB_NODE_ID     0x00
#define MS_MAX_RETRIES     3
#define MS_ACK_TIMEOUT_MS  2000

/* Node types */
typedef enum {
    MS_NODE_HUB = 0,
    MS_NODE_ACOUSTIC,
    MS_NODE_TRAP,
    MS_NODE_BARRIER,
    MS_NODE_WEATHER,
} ms_node_type_t;

/* Mesh context */
typedef struct {
    uint8_t  node_id;        /* Assigned by hub */
    uint8_t  node_type;      /* ms_node_type_t */
    uint8_t  tdma_slot;      /* Assigned slot index */
    uint16_t msg_seq;        /* Message sequence counter */
    uint8_t  retry_count;    /* Current retry count */
    int8_t   last_rssi;      /* Last received RSSI */
    uint8_t  joined;         /* 1 if joined network */
    uint8_t  relay_enabled;  /* 1 if acting as relay */
    uint32_t last_hub_seen;  /* Timestamp of last hub contact */
    uint8_t  high_risk_mode;  /* 1 if high-risk mode active */
} ms_mesh_ctx_t;

/* Initialize mesh layer */
int ms_mesh_init(ms_mesh_ctx_t *ctx, uint8_t node_type,
                 const ms_spi_interface_t *spi,
                 const ms_radio_config_t *radio_cfg);

/* Join network (hub assigns node ID and slot) */
int ms_mesh_join(ms_mesh_ctx_t *ctx);

/* Send a message via mesh (with retries) */
int ms_mesh_send(ms_mesh_ctx_t *ctx, const ms_message_t *msg);

/* Receive a message (in assigned TDMA slot) */
int ms_mesh_recv(ms_mesh_ctx_t *ctx, ms_message_t *msg, uint32_t timeout_ms);

/* TDMA: wait for our assigned slot */
void ms_mesh_wait_slot(ms_mesh_ctx_t *ctx);

/* Relay a message (for out-of-range nodes) */
int ms_mesh_relay(ms_mesh_ctx_t *ctx, const ms_message_t *msg);

/* Hub: assign slot to joining node */
int ms_mesh_hub_assign_slot(ms_mesh_ctx_t *ctx, uint8_t node_type,
                            uint8_t *assigned_id, uint8_t *assigned_slot);

/* Hub: broadcast time sync */
int ms_mesh_hub_time_sync(ms_mesh_ctx_t *ctx, uint32_t epoch);

#endif /* MOSQUITOSYNC_MESH_H */