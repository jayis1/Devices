/**
 * SightSync Vision Hub — BLE Central (connects to Eye Tag)
 * License: MIT
 */

#ifndef BLE_CENTRAL_H
#define BLE_CENTRAL_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/protocol.h"

typedef void (*ble_central_rx_cb_t)(const sightsync_header_t *hdr,
                                     const uint8_t *payload);

void ble_central_init(ble_central_rx_cb_t rx_cb);
void ble_central_start_scan(void);
void ble_central_stop_scan(void);
bool ble_central_is_connected(void);
void ble_central_send(const uint8_t *data, uint8_t len);

#endif /* BLE_CENTRAL_H */