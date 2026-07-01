/**
 * MigraineSync — Hub BLE Central Header
 * =====================================
 * License: MIT
 */

#ifndef HUB_BLE_CENTRAL_H
#define HUB_BLE_CENTRAL_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/protocol.h"

/**
 * Initialize BLE stack and start scanning for MigraineSync nodes.
 */
int ble_central_init(void);

/**
 * Send a frame to a specific node via BLE.
 */
int ble_send(uint16_t node_id, const frame_t *frame);

/**
 * Check if a node is currently connected.
 */
bool ble_is_connected(uint16_t node_id);

/**
 * Get the most recent frame received from any node.
 * Returns 0 if frame available, -1 if queue empty.
 */
int ble_recv(frame_t *out_frame, tlv_t *out_tlvs, int *out_n_tlvs, uint32_t timeout_ms);

#endif /* HUB_BLE_CENTRAL_H */