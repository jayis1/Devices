/*
 * PostureSync Hub — TDMA Coordinator
 * SX1262 Sub-GHz 868 MHz mesh coordination logic
 */

#include "tdma_coordinator.h"
#include "../common/protocol.h"
#include "../common/mesh.h"
#include <string.h>

void tdma_send_beacon(mesh_state_t *state, sx1262_handle_t *radio, uint32_t timestamp)
{
    beacon_t beacon = {0};
    mesh_build_beacon(state, &beacon, timestamp);

    postsync_frame_t frame;
    postsync_build_frame(&frame, NODE_ID_HUB, NODE_ID_BROADCAST,
                         MSG_TYPE_BEACON, state->tx_seq++,
                         (uint8_t *)&beacon, sizeof(beacon));

    uint8_t buf[FRAME_MAX_LEN];
    memcpy(buf, &frame, sizeof(frame));
    sx1262_tx(radio, buf, sizeof(frame));
}

void tdma_handle_join(mesh_state_t *state, sx1262_handle_t *radio, const postsync_frame_t *req)
{
    join_req_t *join = (join_req_t *)req->payload;
    mesh_add_node(state, req->src_id, join->node_type);

    /* Send ACK with slot assignment */
    uint8_t ack_payload[2] = {
        mesh_assign_slot(state, req->src_id),
        0x00
    };

    postsync_frame_t ack;
    postsync_build_frame(&ack, NODE_ID_HUB, req->src_id,
                         MSG_TYPE_JOIN_ACK, state->tx_seq++,
                         ack_payload, sizeof(ack_payload));

    uint8_t buf[FRAME_MAX_LEN];
    memcpy(buf, &ack, sizeof(ack));
    sx1262_tx(radio, buf, sizeof(ack));
}

void tdma_broadcast_config(mesh_state_t *state, sx1262_handle_t *radio,
                           uint8_t target_node, const uint8_t *config, uint8_t len)
{
    postsync_frame_t frame;
    postsync_build_frame(&frame, NODE_ID_HUB, target_node,
                         MSG_TYPE_CONFIG, state->tx_seq++,
                         config, len);

    uint8_t buf[FRAME_MAX_LEN];
    memcpy(buf, &frame, sizeof(frame));
    sx1262_tx(radio, buf, sizeof(frame));
}

void tdma_send_alert(mesh_state_t *state, sx1262_handle_t *radio,
                    uint16_t target, const posture_alert_t *alert)
{
    postsync_frame_t frame;
    postsync_build_frame(&frame, NODE_ID_HUB, target,
                         MSG_TYPE_POSTURE_ALERT, state->tx_seq++,
                         (const uint8_t *)alert, sizeof(*alert));

    uint8_t buf[FRAME_MAX_LEN];
    memcpy(buf, &frame, sizeof(frame));
    sx1262_tx(radio, buf, sizeof(frame));
}