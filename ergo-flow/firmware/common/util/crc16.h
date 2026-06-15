/*
 * ErgoFlow — CRC16 CCITT Header
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

/* Compute CRC16-CCITT over a buffer (init=0xFFFF, poly=0x1021) */
uint16_t crc16_ccitt(const uint8_t *data, uint16_t len);

/* Update CRC16 incrementally (for streaming) */
uint16_t crc16_ccitt_update(uint16_t crc, uint8_t byte);

#endif /* CRC16_H */