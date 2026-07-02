/**
 * JointSync Joint Tag — BLE Peripheral Interface
 *
 * License: MIT
 */

#ifndef BLE_PERIPH_H
#define BLE_PERIPH_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*ble_cb_t)(const uint8_t *data, uint8_t len);
typedef void (*ble_connect_cb_t)(void);
typedef void (*ble_disconnect_cb_t)(void);

void ble_periph_init(ble_cb_t cmd_cb,
                     ble_connect_cb_t connect_cb,
                     ble_disconnect_cb_t disconnect_cb);
void ble_periph_advertise(void);
void ble_periph_send(uint8_t *data, uint8_t len);
bool ble_periph_is_connected(void);

#endif /* BLE_PERIPH_H */