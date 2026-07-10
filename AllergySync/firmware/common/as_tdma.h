/*
 * AllergySync — TDMA Mesh MAC
 * Time-division multiple access mesh layer above LR1121 radio.
 *
 * The hub is the coordinator:
 *   - Slot 0: Beacon (hub → all, contains slot assignments + time sync)
 *   - Slots 1-11: Node data slots (assigned during join)
 *
 * Nodes synchronize to beacon and transmit only in their assigned slot.
 * Mesh forwarding: a packet with hop_count > 0 is re-broadcast by
 * intermediate nodes in their slot.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef AS_TDMA_H
#define AS_TDMA_H

#include "allergysync_proto.h"
#include "as_lr1121.h"
#include <stdbool.h>

/* ---- TDMA node state ---- */
typedef struct {
    uint8_t  node_id;
    uint8_t  slot;           /* Assigned TDMA slot (0 = unassigned) */
    bool     is_hub;
    bool     synced;         /* Time-synced to hub beacon */
    uint32_t last_beacon_ms; /* Local timer at last beacon RX */
    uint32_t frame_offset_ms; /* Offset for slot timing */
    uint16_t seq_counter;
    uint8_t  session_key[AS_AES_KEY_LEN];
    uint8_t  nonce[13];
} as_tdma_node_t;

/* ---- Callbacks ---- */
typedef void (*as_tdma_rx_cb)(const as_header_t *hdr,
                              const uint8_t *payload, size_t len,
                              int8_t rssi);

/*
 * Initialize TDMA layer.
 *   node: pre-allocated node context
 *   is_hub: true if this node is the coordinator
 *   radio: initialized LR1121 port
 */
void as_tdma_init(as_tdma_node_t *node, bool is_hub,
                  const as_lr1121_port_t *radio);

/*
 * Hub: send beacon with current slot assignments.
 */
void as_tdma_hub_send_beacon(as_tdma_node_t *node,
                             uint16_t slot_bitmap,
                             uint8_t active_nodes,
                             uint32_t unix_time);

/*
 * Node: process received packet (called from radio RX callback).
 * Handles beacon sync, join handshake, and passes data to app callback.
 */
void as_tdma_handle_rx(as_tdma_node_t *node,
                       const uint8_t *raw, size_t len,
                       int8_t rssi,
                       as_tdma_rx_cb app_cb);

/*
 * Node: send data in assigned slot.
 * Returns 0 on success, -1 if not synced or slot not assigned.
 */
int as_tdma_send(as_tdma_node_t *node, uint8_t msg_type,
                 uint8_t dst_id,
                 const uint8_t *payload, uint16_t payload_len);

/*
 * Node: send join request to hub.
 */
void as_tdma_join(as_tdma_node_t *node, uint8_t node_type,
                  const uint8_t *pubkey);

/*
 * Get time until next assigned slot (ms).
 */
uint32_t as_tdma_time_to_slot(as_tdma_node_t *node);

#endif /* AS_TDMA_H */