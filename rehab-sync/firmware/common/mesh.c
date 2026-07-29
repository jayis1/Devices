/*
 * RehabSync — TDMA Mesh Network Layer
 * Coordinator (Hub) manages slot assignments; nodes join and transmit in slots.
 */
#include "mesh.h"
#include "config.h"
#include <string.h>

int rs_mesh_init(rs_mesh_ctx_t *mesh, uint8_t self_id, uint8_t self_type,
                 bool is_coordinator, sx1262_t *radio)
{
    memset(mesh, 0, sizeof(*mesh));
    mesh->self_id = self_id;
    mesh->self_type = self_type;
    mesh->is_coordinator = is_coordinator;
    mesh->radio = radio;
    mesh->my_slot = 0;
    mesh->frame_counter = 0;
    mesh->seq_counter = 0;
    mesh->joined = is_coordinator; /* coordinator is always "joined" */

    if (is_coordinator) {
        /* Coordinator takes slot 0 */
        mesh->node_table[0].node_id = self_id;
        mesh->node_table[0].node_type = self_type;
        mesh->node_table[0].slot = 0;
        mesh->node_table[0].active = true;
        mesh->node_table[0].rssi = 0;
        mesh->node_table[0].last_seen = 0;
    }

    return 0;
}

int rs_mesh_assign_slot(rs_mesh_ctx_t *mesh, uint8_t node_id, uint8_t node_type)
{
    /* Find existing or free slot */
    for (int i = 1; i < RS_SLOT_COUNT; i++) {
        if (mesh->node_table[i].node_id == node_id) {
            mesh->node_table[i].node_type = node_type;
            mesh->node_table[i].active = true;
            mesh->node_table[i].last_seen = mesh->frame_counter;
            return i;
        }
    }
    for (int i = 1; i < RS_SLOT_COUNT; i++) {
        if (!mesh->node_table[i].active) {
            mesh->node_table[i].node_id = node_id;
            mesh->node_table[i].node_type = node_type;
            mesh->node_table[i].slot = i;
            mesh->node_table[i].active = true;
            mesh->node_table[i].last_seen = mesh->frame_counter;
            return i;
        }
    }
    return -1; /* no free slots */
}

int rs_mesh_join(rs_mesh_ctx_t *mesh, uint32_t timeout_ms)
{
    if (mesh->is_coordinator || mesh->joined) return 0;

    /* Send JOIN_REQ to coordinator (broadcast) */
    uint8_t payload[2] = { mesh->self_id, mesh->self_type };
    uint8_t msg[RS_MAX_MSG];
    size_t msg_len = rs_encode(msg, sizeof(msg),
                               mesh->self_id, RS_BROADCAST,
                               RS_MSG_JOIN_REQ, 0,
                               mesh->seq_counter++, payload, 2);

    int ret = sx1262_tx(mesh->radio, msg, msg_len, 1000);
    if (ret < 0) return -1;

    /* Wait for JOIN_ACK */
    rs_msg_header_t hdr;
    uint8_t resp_payload[RS_MAX_PAYLOAD];
    ret = sx1262_rx(mesh->radio, resp_payload, sizeof(resp_payload), timeout_ms);
    if (ret < 0) return -2;

    /* Parse response */
    int plen = rs_decode(resp_payload, ret, &hdr, payload, sizeof(payload));
    if (plen < 0) return -3;
    if (hdr.msg_type != RS_MSG_JOIN_ACK) return -4;
    if (plen < 1) return -5;

    mesh->my_slot = payload[0];
    mesh->joined = true;
    return 0;
}

int rs_mesh_send(rs_mesh_ctx_t *mesh, uint8_t dst_id,
                 uint8_t msg_type, uint8_t subtype,
                 const uint8_t *payload, size_t len)
{
    uint8_t msg[RS_MAX_MSG];
    size_t msg_len = rs_encode(msg, sizeof(msg),
                               mesh->self_id, dst_id,
                               msg_type, subtype,
                               mesh->seq_counter++, payload, len);
    if (msg_len == 0) return -1;

    int ret = sx1262_tx(mesh->radio, msg, msg_len, 2000);
    return (ret > 0) ? (int)msg_len : -1;
}

int rs_mesh_broadcast(rs_mesh_ctx_t *mesh, uint8_t msg_type, uint8_t subtype,
                      const uint8_t *payload, size_t len)
{
    return rs_mesh_send(mesh, RS_BROADCAST, msg_type, subtype, payload, len);
}

int rs_mesh_recv(rs_mesh_ctx_t *mesh, rs_msg_header_t *hdr,
                 uint8_t *payload, size_t cap, uint32_t timeout_ms)
{
    uint8_t msg[RS_MAX_MSG];
    int ret = sx1262_rx(mesh->radio, msg, sizeof(msg), timeout_ms);
    if (ret < 0) return ret;

    int plen = rs_decode(msg, ret, hdr, payload, cap);
    if (plen < 0) return plen;

    /* Update node table if from known node */
    for (int i = 0; i < RS_SLOT_COUNT; i++) {
        if (mesh->node_table[i].node_id == hdr->src_id) {
            mesh->node_table[i].rssi = mesh->radio->rssi;
            mesh->node_table[i].last_seen = mesh->frame_counter;
            break;
        }
    }

    /* Handle join request if coordinator */
    if (mesh->is_coordinator && hdr->msg_type == RS_MSG_JOIN_REQ) {
        uint8_t node_id = payload[0];
        uint8_t node_type = payload[1];
        int slot = rs_mesh_assign_slot(mesh, node_id, node_type);

        uint8_t ack_payload[1] = { (uint8_t)slot };
        rs_mesh_send(mesh, node_id, RS_MSG_JOIN_ACK, 0, ack_payload, 1);
        return -10; /* handled join, no data for caller */
    }

    return plen;
}

void rs_mesh_heartbeat(rs_mesh_ctx_t *mesh)
{
    uint8_t hb_payload[4] = {
        mesh->self_type,
        (uint8_t)(mesh->frame_counter >> 16),
        (uint8_t)(mesh->frame_counter >> 8),
        (uint8_t)(mesh->frame_counter & 0xFF)
    };
    rs_mesh_broadcast(mesh, RS_MSG_HEARTBEAT, 0, hb_payload, 4);
    mesh->frame_counter++;
}

void rs_mesh_prune_stale(rs_mesh_ctx_t *mesh, uint32_t max_age_s)
{
    uint32_t threshold = mesh->frame_counter - (max_age_s * 1000 / RS_TDMA_SLOT_MS);
    for (int i = 1; i < RS_SLOT_COUNT; i++) {
        if (mesh->node_table[i].active &&
            mesh->node_table[i].last_seen < threshold) {
            mesh->node_table[i].active = false;
            mesh->node_table[i].node_id = 0;
        }
    }
}