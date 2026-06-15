/*
 * ErgoFlow — CRC16 CCITT Implementation
 * Used for UART protocol and data integrity checks
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "crc16.h"

uint16_t crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    const uint16_t polynomial = 0x1021;

    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint16_t crc16_ccitt_update(uint16_t crc, uint8_t byte)
{
    const uint16_t polynomial = 0x1021;
    crc ^= (uint16_t)byte << 8;
    for (uint8_t j = 0; j < 8; j++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ polynomial;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}