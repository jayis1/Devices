/**
 * JointSync Joint Scanner — BLE Interface
 *
 * License: MIT
 */

#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*ble_scanner_cb_t)(uint16_t sender_id, const uint8_t *data, uint8_t len);

void ble_scanner_init(ble_scanner_cb_t cmd_cb);
void ble_scanner_advertise(void);
void ble_scanner_send(uint8_t *data, uint8_t len);
bool ble_scanner_is_connected(void);

#endif /* BLE_SCANNER_H */