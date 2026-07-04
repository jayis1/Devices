#ifndef GLUCOSYNC_BLE_CENTRAL_H
#define GLUCOSYNC_BLE_CENTRAL_H

#include <stdint.h>

/**
 * BLE 5.0 central role for ESP32-S3 hub.
 * Scans for and connects to GlucoSync nodes (Scanner, Band, Pen Tag).
 */

typedef void (*ble_central_rx_cb)(uint16_t sender_id, const uint8_t *data, uint8_t len);

void ble_central_init(ble_central_rx_cb callback);
void ble_central_start_scan(void);
void ble_central_stop_scan(void);
void ble_send_to_scanner(const uint8_t *data, uint8_t len);
void ble_send_to_band(const uint8_t *data, uint8_t len);
void ble_send_to_pen(const uint8_t *data, uint8_t len);
void ble_send_to_all(const uint8_t *data, uint8_t len);

#endif