/*
 * sole_protocol.c — SoleGuard shared mesh protocol implementation
 *
 * CRC-16/CCITT, payload packing, zone helpers.
 *
 * SPDX-License-Identifier: MIT
 */
#include "sole_protocol.h"

#define CRC16_POLY 0x1021u

uint16_t sole_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000u)
                crc = (crc << 1) ^ CRC16_POLY;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* The crc16 field is the LAST 2 bytes of every payload struct.
 * struct_size_without_crc = sizeof(struct) - 2.
 */
uint16_t sole_pack_crc(void *payload, size_t struct_size_without_crc)
{
    uint16_t crc = sole_crc16((const uint8_t *)payload, struct_size_without_crc);
    /* write crc into the trailing 2 bytes (little-endian) */
    uint8_t *p = (uint8_t *)payload + struct_size_without_crc;
    p[0] = (uint8_t)(crc & 0xFF);
    p[1] = (uint8_t)((crc >> 8) & 0xFF);
    return crc;
}

int sole_verify_crc(const void *payload, size_t struct_size_without_crc, uint16_t crc)
{
    uint16_t calc = sole_crc16((const uint8_t *)payload, struct_size_without_crc);
    return (calc == crc) ? 0 : -1;
}

/* 24 sensors -> 6 zones (4 sensors each, contiguous) */
uint8_t sole_zone_of_sensor(uint8_t sensor_idx)
{
    if (sensor_idx >= 24) return 0;
    return sensor_idx / 4;   /* 0-5 */
}

uint8_t sole_zone_peak_pressure(const uint8_t pressure[24], sole_zone_t zone)
{
    uint8_t peak = 0;
    uint8_t base = (uint8_t)zone * 4u;
    for (int i = 0; i < 4; i++) {
        if (pressure[base + i] > peak)
            peak = pressure[base + i];
    }
    return peak;
}

/* Sum of pressure across samples for PTI proxy (caller divides by scale) */
uint32_t sole_zone_pti_sum(const uint8_t pressure[24], sole_zone_t zone, uint8_t samples)
{
    uint32_t sum = 0;
    uint8_t base = (uint8_t)zone * 4u;
    for (int i = 0; i < 4; i++) {
        sum += (uint32_t)pressure[base + i] * (uint32_t)samples;
    }
    return sum;
}