/*
 * mesh_protocol.c — Shared BLE mesh protocol implementation for SleepSync
 *
 * CRC-16/CCITT, message building/parsing utilities
 */

#include "mesh_protocol.h"
#include <string.h>

/* CRC-16/CCITT (polynomial 0x1021, init 0xFFFF) */
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

/*
 * Mesh message format:
 * [ SYNC(2) | LEN(1) | SRC(1) | DST(1) | TYPE(1) | PAYLOAD(0-50) | CRC16(2) ]
 *
 * SYNC:    0x5YNC (0x595E)
 * LEN:     payload length (0-50)
 * SRC/DST: node IDs
 * TYPE:    MSG_* message type
 * CRC16:   over LEN+SRC+DST+TYPE+PAYLOAD
 */
#define MESH_SYNC_WORD  0x595E
#define MESH_HDR_SIZE   6   /* sync(2) + len(1) + src(1) + dst(1) + type(1) */
#define MESH_CRC_SIZE   2
#define MESH_MAX_PAYLOAD 50

uint16_t mesh_build_message(uint8_t src, uint8_t dst, uint8_t type,
                             const uint8_t *payload, uint8_t payload_len,
                             uint8_t *out, uint16_t out_max)
{
    if (payload_len > MESH_MAX_PAYLOAD)
        return 0;

    uint16_t total = MESH_HDR_SIZE + payload_len + MESH_CRC_SIZE;
    if (total > out_max)
        return 0;

    uint16_t idx = 0;

    /* Sync word */
    out[idx++] = (MESH_SYNC_WORD >> 8) & 0xFF;
    out[idx++] = MESH_SYNC_WORD & 0xFF;

    /* Length */
    out[idx++] = payload_len;

    /* Source ID */
    out[idx++] = src;

    /* Destination ID */
    out[idx++] = dst;

    /* Message type */
    out[idx++] = type;

    /* Payload */
    if (payload_len > 0 && payload != NULL) {
        memcpy(&out[idx], payload, payload_len);
    }
    idx += payload_len;

    /* CRC-16 over len + src + dst + type + payload */
    uint16_t crc = mesh_crc16(&out[2], idx - 2);
    out[idx++] = crc & 0xFF;
    out[idx++] = (crc >> 8) & 0xFF;

    return idx;
}

int8_t mesh_parse_message(const uint8_t *raw, uint16_t raw_len,
                           uint8_t *src, uint8_t *dst, uint8_t *type,
                           uint8_t *payload, uint8_t *payload_len)
{
    if (raw_len < MESH_HDR_SIZE + MESH_CRC_SIZE)
        return -1;

    /* Check sync word */
    uint16_t sync = ((uint16_t)raw[0] << 8) | raw[1];
    if (sync != MESH_SYNC_WORD)
        return -2;

    /* Extract header fields */
    uint8_t len = raw[2];
    *src = raw[3];
    *dst = raw[4];
    *type = raw[5];
    *payload_len = len;

    /* Verify total length */
    uint16_t expected = MESH_HDR_SIZE + len + MESH_CRC_SIZE;
    if (raw_len < expected)
        return -3;

    /* Extract payload */
    if (len > 0) {
        memcpy(payload, &raw[MESH_HDR_SIZE], len);
    }

    /* Verify CRC */
    uint16_t received_crc = raw[MESH_HDR_SIZE + len] |
                           ((uint16_t)raw[MESH_HDR_SIZE + len + 1] << 8);
    uint16_t computed_crc = mesh_crc16(&raw[2], MESH_HDR_SIZE - 2 + len);
    if (received_crc != computed_crc)
        return -4;

    return 0;
}