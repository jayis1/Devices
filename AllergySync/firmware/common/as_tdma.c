/*
 * AllergySync — TDMA Mesh MAC Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "as_tdma.h"
#include "as_lr1121.h"
#include "allergysync_proto.h"
#include <string.h>

static as_lr1121_port_t radio_port;
static as_tdma_node_t *g_node = NULL;

void as_tdma_init(as_tdma_node_t *node, bool is_hub,
                  const as_lr1121_port_t *radio)
{
    memset(node, 0, sizeof(*node));
    node->is_hub = is_hub;
    node->node_id = is_hub ? 0 : 0xFF; /* unassigned */
    node->slot = is_hub ? 0 : 0;      /* hub uses slot 0 */
    node->synced = is_hub;            /* hub is always synced */
    memcpy(&radio_port, radio, sizeof(radio_port));
    g_node = node;
}

void as_tdma_hub_send_beacon(as_tdma_node_t *node,
                             uint16_t slot_bitmap,
                             uint8_t active_nodes,
                             uint32_t unix_time)
{
    as_beacon_t beacon;
    beacon.unix_time    = unix_time;
    beacon.slot_bitmap  = slot_bitmap;
    beacon.active_nodes = active_nodes;
    beacon.flags        = 0;
    beacon.reserved[0]  = 0;

    uint8_t pkt[AS_MAX_PACKET];
    size_t pkt_len;
    as_build_packet(pkt, &pkt_len, AS_MSG_BEACON, 0, 0xFF,
                    (uint8_t *)&beacon, sizeof(beacon));
    /* Hub broadcasts beacon on slot 0 */
    as_lr1121_tx(pkt, pkt_len, 100);
}

void as_tdma_handle_rx(as_tdma_node_t *node,
                       const uint8_t *raw, size_t len,
                       int8_t rssi,
                       as_tdma_rx_cb app_cb)
{
    as_header_t hdr;
    uint8_t payload[AS_MAX_PAYLOAD];
    size_t payload_len;

    if (as_parse_packet(raw, len, &hdr, payload, &payload_len) != 0)
        return;

    /* Sync to beacon */
    if (hdr.msg_type == AS_MSG_BEACON && !node->is_hub) {
        as_beacon_t *beacon = (as_beacon_t *)payload;
        node->synced = true;
        node->last_beacon_ms = 0; /* Platform timer should be set here */
        /* Check if we have a slot assigned */
        if (beacon->slot_bitmap & (1 << node->slot))
            node->synced = true;
        return;
    }

    /* Join response */
    if (hdr.msg_type == AS_MSG_JOIN_RSP && hdr.dst_id == node->node_id) {
        as_join_rsp_t *rsp = (as_join_rsp_t *)payload;
        node->node_id = rsp->assigned_id;
        node->slot = rsp->slot;
        memcpy(node->session_key, rsp->session_key, AS_AES_KEY_LEN);
        return;
    }

    /* Forward to app callback if addressed to us or broadcast */
    if (hdr.dst_id == node->node_id || hdr.dst_id == 0xFF) {
        if (app_cb)
            app_cb(&hdr, payload, payload_len, rssi);
    }
}

int as_tdma_send(as_tdma_node_t *node, uint8_t msg_type,
                 uint8_t dst_id,
                 const uint8_t *payload, uint16_t payload_len)
{
    if (!node->synced || node->slot == 0)
        return -1;

    uint8_t pkt[AS_MAX_PACKET];
    size_t pkt_len;
    as_build_packet(pkt, &pkt_len, msg_type, node->node_id, dst_id,
                    payload, payload_len);

    /* Set sequence number */
    as_header_t *hdr = (as_header_t *)pkt;
    hdr->seq = node->seq_counter++;

    /* Wait for our TDMA slot (platform-specific timer) */
    /* In real implementation, would sleep until slot time */
    uint32_t wait = as_tdma_time_to_slot(node);
    if (wait > 0)
        radio_port.delay_ms(wait);

    return as_lr1121_tx(pkt, pkt_len, AS_MESH_SLOT_MS);
}

void as_tdma_join(as_tdma_node_t *node, uint8_t node_type,
                  const uint8_t *pubkey)
{
    as_join_req_t req;
    req.node_type = node_type;
    req.hw_version = 0x10;
    req.fw_version = 0x01;
    memcpy(req.pubkey, pubkey, AS_ECDH_PUBKEY_LEN);

    uint8_t pkt[AS_MAX_PACKET];
    size_t pkt_len;
    as_build_packet(pkt, &pkt_len, AS_MSG_JOIN_REQ, 0xFF, 0x00,
                    (uint8_t *)&req, sizeof(req));

    /* Send in contention window (any slot) */
    as_lr1121_tx(pkt, pkt_len, 100);
}

uint32_t as_tdma_time_to_slot(as_tdma_node_t *node)
{
    if (!node->synced || node->slot == 0)
        return 0;

    /* Calculate time remaining until our slot */
    /* This is platform-specific; in real implementation uses a hardware timer */
    /* Placeholder: return 0 (assume called at slot boundary) */
    (void)node;
    return 0;
}