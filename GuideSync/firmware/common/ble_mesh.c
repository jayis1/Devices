/*
 * GuideSync — BLE Star Network Layer (Implementation)
 * Simplified BLE star layer — in production, uses NimBLE (ESP32-S3)
 * or Zephyr BLE (nRF52840) for actual GATT operations.
 */
#include "ble_mesh.h"
#include <string.h>

static const gs_ble_interface_t *g_ble_if = NULL;

int gs_ble_init(gs_ble_ctx_t *ctx, uint8_t node_type,
                const gs_ble_interface_t *ble_if)
{
    if (!ctx || !ble_if) return -1;

    memset(ctx, 0, sizeof(*ctx));
    ctx->node_type = node_type;
    ctx->node_id = 0xFF; /* Unassigned */
    ctx->joined = 0;
    ctx->msg_seq = 0;

    g_ble_if = ble_if;
    g_ble_if->init();

    return 0;
}

/* Peripheral: advertise and wait for hub to connect */
int gs_ble_join(gs_ble_ctx_t *ctx)
{
    if (!ctx || !g_ble_if) return -1;

    /* Start advertising — hub will scan and connect */
    g_ble_if->advert_start();

    /* Wait for connection (hub connects, sends JOIN_ACK) */
    uint8_t buf[GS_MAX_MSG];
    int rx_len = g_ble_if->recv(buf, sizeof(buf), 10000); /* 10s timeout */

    if (rx_len > 0) {
        gs_message_t msg;
        if (gs_decode(&msg, buf, rx_len) == 0 &&
            msg.header.type == GS_MSG_JOIN_ACK) {
            ctx->node_id = msg.payload[0];
            ctx->joined = 1;
            return 0;
        }
    }

    g_ble_if->advert_stop();
    return -1; /* Join failed */
}

/* Send a message via BLE GATT notify */
int gs_ble_send(gs_ble_ctx_t *ctx, const gs_message_t *msg)
{
    if (!ctx || !msg || !g_ble_if) return -1;

    uint8_t buf[GS_MAX_MSG];
    size_t len = gs_encode(msg, buf, sizeof(buf));
    if (len == 0) return -1;

    return g_ble_if->send(buf, (uint8_t)len) > 0 ? 0 : -1;
}

/* Receive a message via BLE GATT */
int gs_ble_recv(gs_ble_ctx_t *ctx, gs_message_t *msg, uint32_t timeout_ms)
{
    if (!ctx || !msg || !g_ble_if) return -1;

    uint8_t buf[GS_MAX_MSG];
    int rx_len = g_ble_if->recv(buf, sizeof(buf), timeout_ms);

    if (rx_len <= 0) return -1;

    if (gs_decode(msg, buf, rx_len) == 0) {
        return 0;
    }

    return -1;
}

/* Hub: assign node ID to a joining peripheral */
int gs_ble_hub_assign_id(gs_ble_ctx_t *ctx, uint8_t node_type,
                         uint8_t *assigned_id)
{
    if (!ctx || !assigned_id) return -1;

    /* Find next free ID (simplified — production: track ID table) */
    static uint8_t next_id = 1; /* 0 = hub */

    if (next_id >= GS_MAX_NODES)
        return -1; /* Network full */

    *assigned_id = next_id++;
    return 0;
}

/* Hub: broadcast time sync to all peripherals */
int gs_ble_hub_time_sync(gs_ble_ctx_t *ctx, uint32_t epoch)
{
    if (!ctx) return -1;

    gs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.sync[0] = GS_SYNC0;
    msg.header.sync[1] = GS_SYNC1;
    msg.header.src = GS_HUB_NODE_ID;
    msg.header.dst = GS_BROADCAST;
    msg.header.type = GS_MSG_TIME_SYNC;
    msg.header.msg_id = ctx->msg_seq++;

    msg.payload[0] = (uint8_t)(epoch & 0xFF);
    msg.payload[1] = (uint8_t)(epoch >> 8);
    msg.payload[2] = (uint8_t)(epoch >> 16);
    msg.payload[3] = (uint8_t)(epoch >> 24);
    msg.payload_len = 4;

    return gs_ble_send(ctx, &msg);
}