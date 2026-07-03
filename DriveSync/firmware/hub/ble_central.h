#ifndef DRIVESYNC_BLE_CENTRAL_H
#define DRIVESYNC_BLE_CENTRAL_H

#include <stdint.h>

/**
 * BLE 5.0 Central role for DriveSync Dash Hub (ESP32-S3).
 * Connects to Steering Wheel Node, Seat Belt Tag, and OBD-II Dongle.
 */

typedef void (*ble_data_cb_t)(uint16_t sender_id, const uint8_t *data, uint8_t len);

/**
 * Initialize BLE central with data callback.
 */
void ble_central_init(ble_data_cb_t callback);

/**
 * Start scanning for DriveSync peripheral nodes.
 */
void ble_central_start_scan(void);

/**
 * Stop scanning.
 */
void ble_central_stop_scan(void);

/**
 * Send packet to Steering Wheel Node.
 */
void ble_send_to_wheel(const uint8_t *data, uint8_t len);

/**
 * Send packet to Seat Belt Tag.
 */
void ble_send_to_belt(const uint8_t *data, uint8_t len);

/**
 * Send packet to OBD-II Dongle.
 */
void ble_send_to_obd(const uint8_t *data, uint8_t len);

/**
 * Send packet to all connected nodes.
 */
void ble_send_to_all(const uint8_t *data, uint8_t len);

/**
 * Get number of connected nodes.
 */
uint8_t ble_central_get_node_count(void);

#endif /* DRIVESYNC_BLE_CENTRAL_H */