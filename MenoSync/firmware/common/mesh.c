/*
 * MenoSync — BLE + Sub-GHz Mesh Implementation
 */
#include "mesh.h"
#include <string.h>

void ms_mesh_init(ms_mesh_ctx_t *ctx, uint8_t self_id)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->self_id = self_id;
}

int ms_mesh_add_node(ms_mesh_ctx_t *ctx, uint8_t node_id, uint8_t node_type,
                     uint8_t link_type)
{
    if (!ctx) return -1;
    /* Check if already present */
    for (int i = 0; i < ctx->node_count; i++) {
        if (ctx->nodes[i].node_id == node_id) {
            ctx->nodes[i].connected = 1;
            ctx->nodes[i].node_type = node_type;
            ctx->nodes[i].link_type = link_type;
            return 0;
        }
    }
    if (ctx->node_count >= MS_MESH_MAX_CONN)
        return -1;
    ms_mesh_node_t *n = &ctx->nodes[ctx->node_count++];
    n->node_id = node_id;
    n->node_type = node_type;
    n->link_type = link_type;
    n->connected = 1;
    n->battery_pct = 100;
    n->rssi = 0;
    n->last_seen_ms = 0;
    n->pkt_rx_count = 0;
    n->pkt_tx_count = 0;
    return 0;
}

int ms_mesh_remove_node(ms_mesh_ctx_t *ctx, uint8_t node_id)
{
    if (!ctx) return -1;
    for (int i = 0; i < ctx->node_count; i++) {
        if (ctx->nodes[i].node_id == node_id) {
            ctx->nodes[i].connected = 0;
            return 0;
        }
    }
    return -1;
}

ms_mesh_node_t *ms_mesh_find_node(ms_mesh_ctx_t *ctx, uint8_t node_id)
{
    if (!ctx) return NULL;
    for (int i = 0; i < ctx->node_count; i++) {
        if (ctx->nodes[i].node_id == node_id)
            return &ctx->nodes[i];
    }
    return NULL;
}

void ms_mesh_update_node(ms_mesh_ctx_t *ctx, uint8_t node_id,
                         uint8_t battery, uint8_t rssi)
{
    ms_mesh_node_t *n = ms_mesh_find_node(ctx, node_id);
    if (n) {
        n->battery_pct = battery;
        n->rssi = rssi;
        n->pkt_rx_count++;
    }
}

int ms_mesh_count_by_type(ms_mesh_ctx_t *ctx, uint8_t node_type)
{
    if (!ctx) return 0;
    int count = 0;
    for (int i = 0; i < ctx->node_count; i++) {
        if (ctx->nodes[i].connected && ctx->nodes[i].node_type == node_type)
            count++;
    }
    return count;
}

int ms_mesh_check_timeouts(ms_mesh_ctx_t *ctx, uint32_t now_ms, uint32_t timeout_s)
{
    if (!ctx) return 0;
    int timed_out = 0;
    uint32_t timeout_ms = timeout_s * 1000;
    for (int i = 0; i < ctx->node_count; i++) {
        if (ctx->nodes[i].connected) {
            if ((now_ms - ctx->nodes[i].last_seen_ms) > timeout_ms) {
                ctx->nodes[i].connected = 0;
                timed_out++;
            }
        }
    }
    return timed_out;
}