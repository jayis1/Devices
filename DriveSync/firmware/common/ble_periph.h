#ifndef DRIVESYNC_BLE_PERIPH_H
#define DRIVESYNC_BLE_PERIPH_H

#include <stdint.h>

/**
 * BLE 5.0 Peripheral role for DriveSync nodes (nRF52840).
 * Shared by Steering Wheel Node and Seat Belt Tag.
 */

typedef void (*ble_cmd_cb_t)(const uint8_t *data, uint8_t len);
typedef void (*ble_connected_cb_t)(void);
typedef void (*ble_disconnected_cb_t)(void);

/**
 * Initialize BLE peripheral with callbacks.
 */
void ble_periph_init(ble_cmd_cb_t cmd_handler,
                     ble_connected_cb_t connected_cb,
                     ble_disconnected_cb_t disconnected_cb);

/**
 * Start advertising.
 */
void ble_periph_advertise(void);

/**
 * Send data to Hub (notification).
 */
void ble_periph_send(const uint8_t *data, uint8_t len);

/**
 * Check if connected.
 */
bool ble_periph_is_connected(void);

#endif