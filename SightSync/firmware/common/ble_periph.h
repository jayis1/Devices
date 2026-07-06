#ifndef SIGHTSYNC_BLE_PERIPH_H
#define SIGHTSYNC_BLE_PERIPH_H

/**
 * SightSync BLE Peripheral — used by Eye Tag (nRF52840) to connect to Hub.
 *
 * License: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

/* BLE Service & Characteristic UUIDs */
#define SS_BLE_SVC_UUID "8a5f0001-1c30-4c8e-b501-2b3a6c7d8e9f"
#define SS_BLE_CHAR_TX  "8a5f0002-1c30-4c8e-b501-2b3a6c7d8e9f"  /* Notify (node → hub) */
#define SS_BLE_CHAR_RX  "8a5f0003-1c30-4c8e-b501-2b3a6c7d8e9f"  /* Write  (hub → node) */

/* Callback for received packet */
typedef void (*ss_ble_rx_callback_t)(const sightsync_header_t *header,
                                      const uint8_t *payload);

void ss_ble_periph_init(uint16_t node_id, ss_ble_rx_callback_t rx_cb);
void ss_ble_periph_start_advertising(void);
void ss_ble_periph_stop_advertising(void);
void ss_ble_periph_send(const uint8_t *data, uint8_t len);
bool ss_ble_periph_is_connected(void);

#endif /* SIGHTSYNC_BLE_PERIPH_H */