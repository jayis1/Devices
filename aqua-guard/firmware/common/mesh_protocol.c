/*
 * mesh_protocol.c — Shared mesh protocol implementation
 */

#include "mesh_protocol.h"
#include <string.h>

/* CRC-16/CCITT (0x1021 polynomial, init 0xFFFF) */
uint16_t mesh_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

uint16_t mesh_build_packet(uint8_t src, uint8_t dst, uint8_t type,
                            const uint8_t *payload, uint8_t payload_len,
                            mesh_packet_t *out)
{
    if (payload_len > MESH_MAX_PAYLOAD) return 0;

    memset(out, 0xAA, 4);  /* preamble */
    out->sync     = MESH_SYNC_WORD;
    out->len      = payload_len;
    out->src_id   = src;
    out->dst_id   = dst;
    out->pkt_type = type;
    if (payload_len > 0 && payload) {
        memcpy(out->payload, payload, payload_len);
    }

    /* CRC over len + src + dst + type + payload */
    uint16_t crc_len = 1 + 1 + 1 + 1 + payload_len;
    out->crc16 = mesh_crc16(&out->len, crc_len);

    return 8 + payload_len + 2;  /* total packet length */
}

int8_t mesh_parse_packet(const uint8_t *raw, uint16_t raw_len, mesh_packet_t *out)
{
    if (raw_len < 10) return -1;  /* too short */

    /* Check preamble */
    for (int i = 0; i < 4; i++) {
        if (raw[i] != 0xAA) return -2;
    }

    memcpy(out, raw, raw_len);

    /* Verify sync word */
    if (out->sync != MESH_SYNC_WORD) return -3;

    /* Verify payload length */
    if (out->len > MESH_MAX_PAYLOAD) return -4;

    /* Verify CRC */
    uint16_t crc_len = 1 + 1 + 1 + 1 + out->len;
    uint16_t calc_crc = mesh_crc16(&out->len, crc_len);
    if (calc_crc != out->crc16) return -5;

    return 0;
}