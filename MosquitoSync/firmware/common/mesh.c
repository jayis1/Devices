/*
 * MosquitoSync — TDMA Mesh Layer (Implementation)
 * Simplified mesh layer — in production, use RTOS timers for precise slot timing.
 */
#include "mesh.h"
#include <string.h>

static const ms_spi_interface_t *g_mesh_spi = NULL;
static ms_radio_config_t g_mesh_radio_cfg;

int ms_mesh_init(ms_mesh_ctx_t *ctx, uint8_t node_type,
                 const ms_spi_interface_t *spi,
                 const ms_radio_config_t *radio_cfg)
{
    if (!ctx || !spi || !radio_cfg) return -1;

    memset(ctx, 0, sizeof(*ctx));
    ctx->node_type = node_type;
    ctx->node_id = 0xFF; /* Unassigned */
    ctx->tdma_slot = 0xFF;
    ctx->joined = 0;
    ctx->msg_seq = 0;
    ctx->high_risk_mode = 0;

    g_mesh_spi = spi;
    memcpy(&g_mesh_radio_cfg, radio_cfg, sizeof(g_mesh_radio_cfg));

    return ms_radio_init(spi, radio_cfg);
}

int ms_mesh_join(ms_mesh_ctx_t *ctx)
{
    if (!ctx) return -1;

    /* Send JOIN_REQ */
    ms_message_t join_req;
    memset(&join_req, 0, sizeof(join_req));
    join_req.header.sync[0] = MS_SYNC0;
    join_req.header.sync[1] = MS_SYNC1;
    join_req.header.src = 0xFF; /* Unassigned */
    join_req.header.dst = 0x00; /* Hub */
    join_req.header.type = MS_MSG_JOIN_REQ;
    join_req.header.msg_id = ctx->msg_seq++;

    /* Payload: node type, protocol version, capabilities, battery, fw major/minor */
    join_req.payload[0] = ctx->node_type;
    join_req.payload[1] = 1; /* Protocol version 1 */
    join_req.payload[2] = 0xFF; /* Capabilities bitmask */
    join_req.payload[3] = 250; /* Battery (x0.01V) = 2.50V */
    join_req.payload[4] = 1; /* FW major */
    join_req.payload[5] = 0; /* FW minor */
    join_req.payload_len = 6;

    uint8_t buf[MS_MAX_MSG];
    size_t len = ms_encode(&join_req, buf, sizeof(buf));
    ms_radio_tx(buf, (uint8_t)len);

    /* Wait for JOIN_ACK */
    uint8_t rx_buf[MS_MAX_MSG];
    int rx_len = ms_radio_rx(rx_buf, sizeof(rx_buf), MS_ACK_TIMEOUT_MS);

    if (rx_len > 0) {
        ms_message_t ack;
        if (ms_decode(&ack, rx_buf, rx_len) == 0 &&
            ack.header.type == MS_MSG_JOIN_ACK) {
            ctx->node_id = ack.payload[0];
            ctx->tdma_slot = ack.payload[1];
            ctx->joined = 1;
            return 0;
        }
    }

    return -1; /* Join failed */
}

int ms_mesh_send(ms_mesh_ctx_t *ctx, const ms_message_t *msg)
{
    if (!ctx || !msg) return -1;

    uint8_t buf[MS_MAX_MSG];
    size_t len = ms_encode(msg, buf, sizeof(buf));
    if (len == 0) return -1;

    for (int retry = 0; retry < MS_MAX_RETRIES; retry++) {
        int tx_result = ms_radio_tx(buf, (uint8_t)len);
        if (tx_result > 0) {
            ctx->last_rssi = ms_radio_get_rssi();
            return 0;
        }
        /* Wait before retry */
        g_mesh_spi->delay_ms(100);
    }

    return -1;
}

int ms_mesh_recv(ms_mesh_ctx_t *ctx, ms_message_t *msg, uint32_t timeout_ms)
{
    if (!ctx || !msg) return -1;

    uint8_t buf[MS_MAX_MSG];
    int rx_len = ms_radio_rx(buf, sizeof(buf), timeout_ms);

    if (rx_len <= 0) return -1;

    if (ms_decode(msg, buf, rx_len) == 0) {
        ctx->last_rssi = ms_radio_get_rssi();

        /* Check if this is a relay message */
        if (msg->header.type == MS_MSG_MESH_RELAY) {
            /* Extract inner message and process */
            ms_mesh_relay(ctx, msg);
        }
        return 0;
    }

    return -1;
}

void ms_mesh_wait_slot(ms_mesh_ctx_t *ctx)
{
    if (!ctx || ctx->tdma_slot == 0xFF) return;

    /* In production: sync to hub beacon, then delay until assigned slot.
     * Simplified: just wait a proportional delay based on slot index.
     */
    uint32_t wait_ms = (uint32_t)ctx->tdma_slot * MS_SLOT_DURATION;
    g_mesh_spi->delay_ms(wait_ms % MS_FRAME_DURATION);
}

int ms_mesh_relay(ms_mesh_ctx_t *ctx, const ms_message_t *msg)
{
    if (!ctx || !msg) return -1;

    /* Re-encode and transmit the inner message */
    ms_message_t inner;
    if (msg->payload_len < 7) return -1;

    /* Extract inner message from relay payload */
    memset(&inner, 0, sizeof(inner));
    /* The relay payload is a full encoded message — decode it */
    if (ms_decode(&inner, msg->payload, msg->payload_len) != 0)
        return -1;

    /* Forward to hub */
    uint8_t buf[MS_MAX_MSG];
    size_t len = ms_encode(&inner, buf, sizeof(buf));
    return ms_radio_tx(buf, (uint8_t)len) > 0 ? 0 : -1;
}

int ms_mesh_hub_assign_slot(ms_mesh_ctx_t *ctx, uint8_t node_type,
                            uint8_t *assigned_id, uint8_t *assigned_slot)
{
    if (!ctx || !assigned_id || !assigned_slot) return -1;

    /* Find next free slot (simplified — production: track slot table) */
    static uint8_t next_id = 1;
    static uint8_t next_slot = 1;

    if (next_slot >= MS_SLOT_COUNT - 1)
        return -1; /* Network full */

    *assigned_id = next_id++;
    *assigned_slot = next_slot++;
    return 0;
}

int ms_mesh_hub_time_sync(ms_mesh_ctx_t *ctx, uint32_t epoch)
{
    if (!ctx) return -1;

    ms_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.sync[0] = MS_SYNC0;
    msg.header.sync[1] = MS_SYNC1;
    msg.header.src = MS_HUB_NODE_ID;
    msg.header.dst = MS_BROADCAST;
    msg.header.type = MS_MSG_TIME_SYNC;
    msg.header.msg_id = ctx->msg_seq++;

    msg.payload[0] = (uint8_t)(epoch & 0xFF);
    msg.payload[1] = (uint8_t)(epoch >> 8);
    msg.payload[2] = (uint8_t)(epoch >> 16);
    msg.payload[3] = (uint8_t)(epoch >> 24);
    msg.payload_len = 4;

    return ms_mesh_send(ctx, &msg);
}