/**
 * JointSync Hub — BLE Central Interface
 *
 * License: MIT
 */

#ifndef BLE_CENTRAL_H
#define BLE_CENTRAL_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*ble_data_cb_t)(uint16_t sender_id, const uint8_t *data, uint8_t len);

void ble_central_init(ble_data_cb_t callback);
void ble_central_start_scan(void);
void ble_central_stop_scan(void);
void ble_send_to_all(uint8_t *data, uint8_t len);
void ble_send_to_scanner(uint8_t *data, uint8_t len);
void ble_send_to_node(uint16_t sender_id, uint8_t *data, uint8_t len);
uint8_t ble_get_connected_count(void);

#endif /* BLE_CENTRAL_H */