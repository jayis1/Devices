/*
 * EchoSync — TDMA Mesh Layer (Implementation)
 * Time-Division Multiple Access mesh networking for Sub-GHz
 */
#include "mesh.h"
#include "config.h"

static uint8_t next_node_id = 1;
static uint8_t next_slot = 1;
static uint8_t slot_assignments[ES_SLOT_COUNT];

int es_mesh_init(es_mesh_ctx_t *ctx, uint8_t node_type,
                 const es_spi_interface_t *spi,
                 const es_radio_config_t *radio_cfg)
{
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));
    ctx->node_type = node_type;
    ctx->msg_seq = 0;
    ctx->joined = 0;

    if (es_radio_init(NULL, spi, radio_cfg) != 0)
        return -1;

    return 0;
}

int es_mesh_join(es_mesh_ctx_t *ctx)
{
    if (!ctx) return -1;

    /* Build JOIN_REQ */
    es_message_t join;
    memset(&join, 0, sizeof(join));
    join.header.sync[0] = ES_SYNC0;
    join.header.sync[1] = ES_SYNC1;
    join.header.src = 0xFF; /* unassigned */
    join.header.dst = 0x00; /* hub */
    join.header.type = ES_MSG_JOIN_REQ;
    join.header.msg_id = ctx->msg_seq++;
    join.payload[0] = ctx->node_type;
    join.payload[1] = 0; /* battery (placeholder) */
    join.payload[2] = 1;  /* fw version */
    join.payload_len = 3;

    /* Send with retries */
    for (int attempt = 0; attempt < ES_MAX_RETRIES; attempt++) {
        uint8_t buf[ES_MAX_MSG];
        size_t len = es_encode(&join, buf, sizeof(buf));
        es_radio_tx(buf, (uint8_t)len);

        /* Wait for JOIN_ACK */
        uint8_t rx_buf[ES_MAX_MSG];
        int rx_len = es_radio_rx(rx_buf, sizeof(rx_buf), ES_ACK_TIMEOUT_MS);
        if (rx_len > 0) {
            es_message_t resp;
            if (es_decode(&resp, rx_buf, rx_len) == 0 &&
                resp.header.type == ES_MSG_JOIN_ACK) {
                ctx->node_id = resp.payload[0];
                ctx->tdma_slot = resp.payload[1];
                ctx->joined = 1;
                return 0;
            }
        }
    }
    return -1;
}

int es_mesh_send(es_mesh_ctx_t *ctx, const es_message_t *msg)
{
    if (!ctx || !msg) return -1;

    uint8_t buf[ES_MAX_MSG];
    size_t len = es_encode(msg, buf, sizeof(buf));

    for (int attempt = 0; attempt < ES_MAX_RETRIES; attempt++) {
        es_radio_tx(buf, (uint8_t)len);

        /* Check for ACK if not broadcast */
        if (msg->header.dst != ES_BROADCAST) {
            uint8_t rx_buf[ES_MAX_MSG];
            int rx_len = es_radio_rx(rx_buf, sizeof(rx_buf), ES_ACK_TIMEOUT_MS);
            if (rx_len > 0) {
                es_message_t resp;
                if (es_decode(&resp, rx_buf, rx_len) == 0 &&
                    resp.header.type == ES_MSG_CMD_ACK)
                    return 0;
            }
        } else {
            return 0;
        }
        ctx->retry_count++;
    }
    return -1;
}

int es_mesh_recv(es_mesh_ctx_t *ctx, es_message_t *msg, uint32_t timeout_ms)
{
    if (!ctx || !msg) return -1;

    uint8_t rx_buf[ES_MAX_MSG];
    int rx_len = es_radio_rx(rx_buf, sizeof(rx_buf), timeout_ms);
    if (rx_len <= 0)
        return -1;

    if (es_decode(msg, rx_buf, rx_len) != 0)
        return -1;

    /* Check if message is for us or broadcast */
    if (msg->header.dst != ctx->node_id && msg->header.dst != ES_BROADCAST)
        return -1;

    ctx->last_rssi = es_radio_get_rssi();
    ctx->last_hub_seen = 0; /* In production: use RTC */
    return 0;
}

void es_mesh_wait_slot(es_mesh_ctx_t *ctx)
{
    if (!ctx) return;
    /* In production: use RTC + DS3231 to align to slot boundary */
    /* Simplified: delay based on slot assignment */
    uint32_t delay_ms = ctx->tdma_slot * ES_SLOT_DURATION;
    /* es_platform_delay(delay_ms); */
    (void)delay_ms;
}

int es_mesh_relay(es_mesh_ctx_t *ctx, const es_message_t *msg)
{
    if (!ctx || !msg) return -1;
    if (msg->header.type != ES_MSG_MESH_RELAY) return -1;

    /* Re-encode with relay flag and send */
    es_message_t relayed = *msg;
    relayed.header.src = ctx->node_id;
    relayed.header.msg_id = ctx->msg_seq++;

    uint8_t buf[ES_MAX_MSG];
    size_t len = es_encode(&relayed, buf, sizeof(buf));
    return es_radio_tx(buf, (uint8_t)len) > 0 ? 0 : -1;
}

int es_mesh_hub_assign_slot(es_mesh_ctx_t *ctx, uint8_t node_type,
                            uint8_t *assigned_id, uint8_t *assigned_slot)
{
    if (!ctx || !assigned_id || !assigned_slot) return -1;

    if (next_node_id >= ES_MAX_NODES || next_slot >= ES_SLOT_COUNT)
        return -1;

    /* Check limits per type */
    if (node_type == ES_NODE_SENTINEL) {
        uint8_t count = 0;
        for (int i = 0; i < ES_SLOT_COUNT; i++)
            if (slot_assignments[i] == ES_NODE_SENTINEL)
                count++;
        if (count >= ES_MESH_MAX_SENTINELS)
            return -1;
    }

    *assigned_id = next_node_id++;
    *assigned_slot = next_slot++;
    slot_assignments[*assigned_slot] = node_type;
    return 0;
}

int es_mesh_hub_time_sync(es_mesh_ctx_t *ctx, uint32_t epoch)
{
    if (!ctx) return -1;

    es_message_t sync;
    memset(&sync, 0, sizeof(sync));
    sync.header.sync[0] = ES_SYNC0;
    sync.header.sync[1] = ES_SYNC1;
    sync.header.src = ES_HUB_NODE_ID;
    sync.header.dst = ES_BROADCAST;
    sync.header.type = ES_MSG_TIME_SYNC;
    sync.header.msg_id = ctx->msg_seq++;
    sync.payload[0] = (uint8_t)(epoch & 0xFF);
    sync.payload[1] = (uint8_t)(epoch >> 8);
    sync.payload[2] = (uint8_t)(epoch >> 16);
    sync.payload[3] = (uint8_t)(epoch >> 24);
    sync.payload_len = 4;

    uint8_t buf[ES_MAX_MSG];
    size_t len = es_encode(&sync, buf, sizeof(buf));
    return es_radio_tx(buf, (uint8_t)len) > 0 ? 0 : -1;
}