/*
 * StormSync — TDMA Mesh Layer (Header)
 * Time-Division Multiple Access mesh networking for Sub-GHz
 */
#ifndef STORMSYNC_MESH_H
#define STORMSYNC_MESH_H

#include "protocol.h"
#include "sx1262.h"

#define SS_SLOT_COUNT      24    /* Total TDMA slots */
#define SS_SLOT_DURATION   50    /* ms per slot */
#define SS_FRAME_DURATION  (SS_SLOT_COUNT * SS_SLOT_DURATION) /* 1200 ms */
#define SS_HUB_NODE_ID     0x00
#define SS_MAX_RETRIES     3
#define SS_ACK_TIMEOUT_MS  2000

/* Node types */
typedef enum {
    SS_NODE_HUB = 0,
    SS_NODE_SUMP,
    SS_NODE_SOIL,
    SS_NODE_WEATHER,
    SS_NODE_ACTUATOR,
} ss_node_type_t;

/* Mesh context */
typedef struct {
    uint8_t  node_id;        /* Assigned by hub */
    uint8_t  node_type;      /* ss_node_type_t */
    uint8_t  tdma_slot;      /* Assigned slot index */
    uint16_t msg_seq;        /* Message sequence counter */
    uint8_t  retry_count;    /* Current retry count */
    int8_t   last_rssi;      /* Last received RSSI */
    uint8_t  joined;         /* 1 if joined network */
    uint8_t  relay_enabled;  /* 1 if acting as relay */
    uint32_t last_hub_seen;  /* Timestamp of last hub contact */
    uint8_t  storm_mode;     /* 1 if storm mode active */
} ss_mesh_ctx_t;

/* Initialize mesh layer */
int ss_mesh_init(ss_mesh_ctx_t *ctx, uint8_t node_type,
                 const ss_spi_interface_t *spi,
                 const ss_radio_config_t *radio_cfg);

/* Join network (hub assigns node ID and slot) */
int ss_mesh_join(ss_mesh_ctx_t *ctx);

/* Send a message via mesh (with retries) */
int ss_mesh_send(ss_mesh_ctx_t *ctx, const ss_message_t *msg);

/* Receive a message (in assigned TDMA slot) */
int ss_mesh_recv(ss_mesh_ctx_t *ctx, ss_message_t *msg, uint32_t timeout_ms);

/* TDMA: wait for our assigned slot */
void ss_mesh_wait_slot(ss_mesh_ctx_t *ctx);

/* Relay a message (for out-of-range nodes) */
int ss_mesh_relay(ss_mesh_ctx_t *ctx, const ss_message_t *msg);

/* Hub: assign slot to joining node */
int ss_mesh_hub_assign_slot(ss_mesh_ctx_t *ctx, uint8_t node_type,
                            uint8_t *assigned_id, uint8_t *assigned_slot);

/* Hub: broadcast time sync */
int ss_mesh_hub_time_sync(ss_mesh_ctx_t *ctx, uint32_t epoch);

#endif /* STORMSYNC_MESH_H */