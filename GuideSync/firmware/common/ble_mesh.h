/*
 * GuideSync — BLE Star Network Layer (Header)
 * BLE 5.0 star topology: Hub = central, nodes = peripherals
 *
 * On ESP32-S3: uses Bluedroid/NimBLE stack
 * On nRF52840: uses Zephyr BLE or nRF SDK SoftDevice
 */
#ifndef GUIDESYNC_BLE_MESH_H
#define GUIDESYNC_BLE_MESH_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"

/* Node context */
typedef struct {
    uint8_t  node_type;       /* GS_NODE_HUB, GS_NODE_GLASSES, etc. */
    uint8_t  node_id;         /* Assigned by hub (0xFF = unassigned) */
    uint8_t  joined;          /* 1 = joined network */
    uint16_t msg_seq;         /* Message sequence counter */
    int8_t   last_rssi;       /* Last BLE RSSI */
    uint8_t  battery_v;       /* Battery voltage (x0.01V) */
} gs_ble_ctx_t;

/* Platform BLE interface (implemented per-node) */
typedef struct {
    void (*init)(void);
    void (*advert_start)(void);         /* Peripheral: start advertising */
    void (*advert_stop)(void);
    void (*scan_start)(void);           /* Central: start scanning */
    void (*scan_stop)(void);
    int  (*connect)(uint8_t *peer_addr); /* Central: connect to peer */
    int  (*send)(const uint8_t *data, uint8_t len);  /* Send via GATT notify */
    int  (*recv)(uint8_t *buf, uint8_t max_len, uint32_t timeout_ms);
    void (*delay_ms)(uint32_t ms);
    void (*disconnect)(void);
} gs_ble_interface_t;

/* Initialize BLE context */
int gs_ble_init(gs_ble_ctx_t *ctx, uint8_t node_type,
                const gs_ble_interface_t *ble_if);

/* Peripheral: join network (advertise, wait for hub connection) */
int gs_ble_join(gs_ble_ctx_t *ctx);

/* Send a message via BLE GATT */
int gs_ble_send(gs_ble_ctx_t *ctx, const gs_message_t *msg);

/* Receive a message via BLE GATT */
int gs_ble_recv(gs_ble_ctx_t *ctx, gs_message_t *msg, uint32_t timeout_ms);

/* Hub: assign node ID to a joining peripheral */
int gs_ble_hub_assign_id(gs_ble_ctx_t *ctx, uint8_t node_type,
                         uint8_t *assigned_id);

/* Hub: broadcast time sync */
int gs_ble_hub_time_sync(gs_ble_ctx_t *ctx, uint32_t epoch);

#endif /* GUIDESYNC_BLE_MESH_H */