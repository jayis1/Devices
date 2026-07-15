/*
 * LawnSync — TDMA Mesh Layer
 * Time-Division Multiple Access mesh networking for Sub-GHz
 *
 * Hub assigns time slots to nodes. Each node transmits only in its slot.
 * Nodes can relay messages for out-of-range peers.
 */
#include "mesh.h"
#include <string.h>

/* Global radio handle — in real implementation this is per-node */
static ls_radio_config_t g_radio_cfg;
static const ls_spi_interface_t *g_spi_iface;

int ls_mesh_init(ls_mesh_ctx_t *ctx, uint8_t node_type,
                 const ls_spi_interface_t *spi,
                 const ls_radio_config_t *radio_cfg)
{
    if (!ctx || !spi || !radio_cfg) return -1;

    memset(ctx, 0, sizeof(*ctx));
    ctx->node_type = node_type;
    ctx->node_id = 0xFF;  /* unassigned */
    ctx->tdma_slot = 0xFF;
    ctx->msg_seq = 0;
    ctx->retry_count = 0;
    ctx->last_rssi = 0;
    ctx->joined = 0;
    ctx->relay_enabled = 0;
    ctx->last_hub_seen = 0;

    g_spi_iface = spi;
    g_radio_cfg = *radio_cfg;

    /* Initialize radio */
    if (ls_radio_init(spi, radio_cfg) != 0) return -1;

    return 0;
}

int ls_mesh_join(ls_mesh_ctx_t *ctx)
{
    if (!ctx) return -1;

    /* Build JOIN_REQ message */
    ls_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.sync[0] = LS_SYNC0;
    msg.header.sync[1] = LS_SYNC1;
    msg.header.src = 0xFF;  /* unassigned */
    msg.header.dst = LS_BROADCAST;
    msg.header.type = LS_MSG_JOIN_REQ;
    msg.header.msg_id = ctx->msg_seq++;

    /* Payload: node type + capabilities + battery + FW version */
    msg.payload[0] = ctx->node_type;
    msg.payload[1] = 0x01;  /* protocol version */
    msg.payload[2] = 0x03;  /* sensor capabilities bitmask */
    msg.payload[3] = 0x64;  /* battery: 3.2V → 0x64=100 (scaled) */
    msg.payload[4] = 0x01;  /* FW version major */
    msg.payload[5] = 0x00;  /* FW version minor */
    msg.payload_len = 6;

    /* Try up to 5 times to join */
    for (int attempt = 0; attempt < 5; attempt++) {
        uint8_t buf[LS_MAX_MSG];
        size_t len = ls_encode(&msg, buf, sizeof(buf));
        if (len == 0) continue;

        /* Listen first (CAD) to avoid collision */
        if (ls_radio_cad(100) == 0) {
            /* Channel clear, send */
            if (ls_radio_tx(buf, (uint8_t)len) > 0) {
                /* Wait for JOIN_ACK from hub */
                ls_radio_packet_t pkt;
                int rx_len = ls_radio_rx(&pkt, LS_ACK_TIMEOUT_MS);
                if (rx_len > 0) {
                    ls_message_t resp;
                    if (ls_decode(&resp, pkt.data, pkt.data_len) == 0) {
                        if (resp.header.type == LS_MSG_JOIN_ACK) {
                            ctx->node_id = resp.payload[0];
                            ctx->tdma_slot = resp.payload[1];
                            ctx->joined = 1;
                            ctx->last_rssi = (int8_t)pkt.rssi;
                            return 0;
                        }
                    }
                }
            }
        }
        /* Wait before retry (random backoff) */
        g_spi_iface->delay_ms(500 + (attempt * 200));
    }

    return -1;
}

int ls_mesh_send(ls_mesh_ctx_t *ctx, const ls_message_t *msg)
{
    if (!ctx || !msg || !ctx->joined) return -1;

    uint8_t buf[LS_MAX_MSG];
    size_t len = ls_encode(msg, buf, sizeof(buf));
    if (len == 0) return -1;

    /* Retransmission with ACK */
    for (int retry = 0; retry < LS_MAX_RETRIES; retry++) {
        /* CAD before TX */
        if (ls_radio_cad(100) == 0) {
            if (ls_radio_tx(buf, (uint8_t)len) > 0) {
                /* Wait for CMD_ACK from hub (for commands) */
                ls_radio_packet_t pkt;
                int rx_len = ls_radio_rx(&pkt, LS_ACK_TIMEOUT_MS);
                if (rx_len > 0) {
                    ls_message_t resp;
                    if (ls_decode(&resp, pkt.data, pkt.data_len) == 0) {
                        ctx->last_rssi = (int8_t)pkt.rssi;
                        if (resp.header.type == LS_MSG_CMD_ACK)
                            return 0; /* ACK'd */
                    }
                }
                /* For telemetry, hub may not ACK each message */
                if (msg->header.type == LS_MSG_TELEMETRY ||
                    msg->header.type == LS_MSG_HEARTBEAT ||
                    msg->header.type == LS_MSG_ALERT) {
                    ctx->last_hub_seen = 0; /* update with real timestamp */
                    return 0; /* fire-and-forget for telemetry */
                }
            }
        }
        g_spi_iface->delay_ms(100 * (retry + 1));
    }

    return -1;
}

int ls_mesh_recv(ls_mesh_ctx_t *ctx, ls_message_t *msg, uint32_t timeout_ms)
{
    if (!ctx || !msg) return -1;

    ls_radio_packet_t pkt;
    int rx_len = ls_radio_rx(&pkt, timeout_ms);
    if (rx_len <= 0) return -1;

    if (ls_decode(msg, pkt.data, pkt.data_len) != 0) return -1;

    /* Check if message is for us or broadcast */
    if (msg->header.dst != ctx->node_id && msg->header.dst != LS_BROADCAST)
        return -1; /* not for us */

    ctx->last_rssi = (int8_t)pkt.rssi;
    return 0;
}

void ls_mesh_wait_slot(ls_mesh_ctx_t *ctx)
{
    if (!ctx || ctx->tdma_slot == 0xFF) return;

    /* In a real implementation, sync with hub's time reference.
     * For simplicity, delay by (slot * SLOT_DURATION) from frame start.
     * The hub sends a TIME_SYNC beacon at the start of each frame.
     */
    uint32_t wait_ms = ctx->tdma_slot * LS_SLOT_DURATION;
    g_spi_iface->delay_ms(wait_ms);
}

int ls_mesh_relay(ls_mesh_ctx_t *ctx, const ls_message_t *msg)
{
    if (!ctx || !msg || !ctx->relay_enabled) return -1;

    /* Re-encode with MESH_RELAY wrapper */
    ls_message_t relay_msg;
    memset(&relay_msg, 0, sizeof(relay_msg));
    relay_msg.header.sync[0] = LS_SYNC0;
    relay_msg.header.sync[1] = LS_SYNC1;
    relay_msg.header.src = ctx->node_id;
    relay_msg.header.dst = LS_HUB_NODE_ID;
    relay_msg.header.type = LS_MSG_MESH_RELAY;
    relay_msg.header.msg_id = ctx->msg_seq++;

    /* Payload: original src + original message */
    relay_msg.payload[0] = msg->header.src;
    relay_msg.payload_len = 1;
    /* Encode original message into relay payload */
    uint8_t orig_buf[LS_MAX_PAYLOAD];
    size_t orig_len = ls_encode(msg, orig_buf, sizeof(orig_buf));
    if (orig_len > 0 && orig_len <= LS_MAX_PAYLOAD - 1) {
        memcpy(&relay_msg.payload[1], orig_buf, orig_len);
        relay_msg.payload_len = 1 + (uint8_t)orig_len;
    }

    /* Send */
    uint8_t buf[LS_MAX_MSG];
    size_t len = ls_encode(&relay_msg, buf, sizeof(buf));
    if (len == 0) return -1;

    /* CAD + TX */
    if (ls_radio_cad(100) == 0) {
        return (ls_radio_tx(buf, (uint8_t)len) > 0) ? 0 : -1;
    }
    return -1;
}

int ls_mesh_hub_assign_slot(ls_mesh_ctx_t *ctx, uint8_t node_type,
                            uint8_t *assigned_id, uint8_t *assigned_slot)
{
    /* Hub maintains a table of assigned nodes.
     * In a full implementation, this searches the table for a free slot.
     * Simplified here: sequential assignment.
     */
    static uint8_t next_id = 1;  /* 0 = hub */
    static uint8_t next_slot = 1;

    *assigned_id = next_id++;
    *assigned_slot = next_slot++;

    if (next_slot >= LS_SLOT_COUNT) {
        return -1; /* network full */
    }
    return 0;
}

int ls_mesh_hub_time_sync(ls_mesh_ctx_t *ctx, uint32_t epoch)
{
    if (!ctx) return -1;

    ls_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.sync[0] = LS_SYNC0;
    msg.header.sync[1] = LS_SYNC1;
    msg.header.src = LS_HUB_NODE_ID;
    msg.header.dst = LS_BROADCAST;
    msg.header.type = LS_MSG_TIME_SYNC;
    msg.header.msg_id = ctx->msg_seq++;

    /* Payload: 4-byte epoch time */
    msg.payload[0] = (uint8_t)(epoch & 0xFF);
    msg.payload[1] = (uint8_t)((epoch >> 8) & 0xFF);
    msg.payload[2] = (uint8_t)((epoch >> 16) & 0xFF);
    msg.payload[3] = (uint8_t)((epoch >> 24) & 0xFF);
    msg.payload_len = 4;

    /* Broadcast (no ACK needed) */
    uint8_t buf[LS_MAX_MSG];
    size_t len = ls_encode(&msg, buf, sizeof(buf));
    if (len == 0) return -1;

    return (ls_radio_tx(buf, (uint8_t)len) > 0) ? 0 : -1;
}