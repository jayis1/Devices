#ifndef DRIVESYNC_BLE_UART_H
#define DRIVESYNC_BLE_UART_H

#include <stdint.h>

/**
 * BLE UART bridge for DriveSync OBD-II Dongle.
 * RP2040 communicates with nRF52832 BLE module via UART.
 * UART0: TX=GP8, RX=GP9, baud=115200.
 *
 * The nRF52832 runs a transparent UART-to-BLE bridge firmware:
 * - Data received on UART is sent as BLE notification to Hub
 * - Data received from Hub is sent on UART to RP2040
 */

typedef void (*ble_cmd_cb_t)(const uint8_t *data, uint8_t len);

/**
 * Initialize BLE UART bridge.
 */
void ble_uart_init(ble_cmd_cb_t cmd_handler);

/**
 * Send data to Hub via BLE UART bridge.
 */
void ble_uart_send(const uint8_t *data, uint8_t len);

/**
 * Process incoming UART data (call in main loop).
 */
void ble_uart_process(void);

/**
 * Check if BLE is connected.
 */
bool ble_uart_is_connected(void);

#endif