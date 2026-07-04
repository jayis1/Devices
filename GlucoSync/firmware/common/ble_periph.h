#ifndef GLUCOSYNC_BLE_PERIPH_H
#define GLUCOSYNC_BLE_PERIPH_H

#include <stdint.h>
#include <stdbool.h>

/**
 * BLE peripheral role for nRF52840 nodes (Activity Band, Pen Tag).
 * Uses Nordic SoftDevice S140.
 */

typedef void (*ble_periph_rx_cb)(const uint8_t *data, uint8_t len);

void ble_periph_init(ble_periph_rx_cb rx_callback);
void ble_periph_start_advertising(const char *device_name);
void ble_periph_stop_advertising(void);
bool ble_periph_is_connected(void);
void ble_periph_send(const uint8_t *data, uint8_t len);
void ble_periph_set_tx_power(int8_t dbm);

#endif