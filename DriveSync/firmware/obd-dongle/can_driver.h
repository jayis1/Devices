#ifndef DRIVESYNC_CAN_DRIVER_H
#define DRIVESYNC_CAN_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * MCP2515 CAN controller driver for DriveSync OBD-II Dongle.
 * SPI interface: CS=GP2, SCK=GP3, MOSI=GP4, MISO=GP5.
 * MCP2551 transceiver connects to OBD-II pins 6 (CAN-H) and 14 (CAN-L).
 */

typedef struct {
    uint32_t id;
    uint8_t  dlc;      /* Data length code (0-8) */
    uint8_t  data[8];  /* Data bytes */
} can_frame_t;

/**
 * Initialize MCP2515 at 500 kbps (standard OBD-II baud rate).
 */
void can_init(void);

/**
 * Send a CAN frame.
 * Returns true on success.
 */
bool can_send(const can_frame_t *frame);

/**
 * Receive a CAN frame (non-blocking).
 * Returns true if a frame was received.
 */
bool can_receive(can_frame_t *frame);

/**
 * Check if a CAN frame is available.
 */
bool can_available(void);

#endif