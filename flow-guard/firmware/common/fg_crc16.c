/**
 * FlowGuard - CRC16 implementation
 * CRC-16/CCITT for packet integrity verification
 *
 * Copyright (c) 2026 jayis1 - MIT License
 */

#include "fg_protocol.h"

/**
 * CRC-16/CCITT-FALSE
 * Polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
 * Init: 0xFFFF
 * RefIn: false, RefOut: false
 * XorOut: 0x0000
 */
uint16_t fg_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint16_t i, j;

    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}