/*
 * AllergySync — Shared Protocol Implementation
 * Platform-independent packet build/parse + CRC16.
 *
 * SPDX-License-Identifier: MIT
 */

#include "allergysync_proto.h"
#include <string.h>

/*
 * CRC-16/CCITT-FALSE — polynomial 0x1021, init 0xFFFF.
 * Used for packet integrity (in addition to AES-CCM MIC).
 */
uint16_t as_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/*
 * Build a packet: header + payload + CRC16.
 * buf must be at least AS_HEADER_LEN + payload_len + 2 bytes.
 */
void as_build_packet(uint8_t *buf, size_t *len, uint8_t msg_type,
                     uint8_t src_id, uint8_t dst_id,
                     const uint8_t *payload, uint16_t payload_len)
{
    as_header_t *hdr = (as_header_t *)buf;
    hdr->version     = AS_PROTO_VERSION;
    hdr->msg_type    = msg_type;
    hdr->src_id      = src_id;
    hdr->dst_id      = dst_id;
    hdr->hop_count   = 0;
    hdr->flags       = 0; /* encryption applied at radio layer */
    hdr->seq         = 0; /* set by caller before TX */
    hdr->payload_len = payload_len;

    if (payload_len > 0 && payload)
        memcpy(buf + AS_HEADER_LEN, payload, payload_len);

    /* Append CRC16 over header + payload */
    uint16_t crc = as_crc16(buf, AS_HEADER_LEN + payload_len);
    buf[AS_HEADER_LEN + payload_len]     = (crc >> 8) & 0xFF;
    buf[AS_HEADER_LEN + payload_len + 1] = crc & 0xFF;

    *len = AS_HEADER_LEN + payload_len + 2;
}

/*
 * Parse a received packet.
 * Returns 0 on success, -1 on error (bad version, bad CRC, too short).
 */
int as_parse_packet(const uint8_t *buf, size_t len, as_header_t *hdr,
                    uint8_t *payload, size_t *payload_len)
{
    if (len < AS_HEADER_LEN + 2)
        return -1;

    memcpy(hdr, buf, AS_HEADER_LEN);

    if (hdr->version != AS_PROTO_VERSION)
        return -1;

    if (hdr->payload_len > AS_MAX_PAYLOAD)
        return -1;

    if (len < (size_t)(AS_HEADER_LEN + hdr->payload_len + 2))
        return -1;

    /* Verify CRC16 */
    uint16_t expected = as_crc16(buf, AS_HEADER_LEN + hdr->payload_len);
    uint16_t received = ((uint16_t)buf[AS_HEADER_LEN + hdr->payload_len] << 8)
                      | buf[AS_HEADER_LEN + hdr->payload_len + 1];
    if (expected != received)
        return -1;

    if (hdr->payload_len > 0) {
        memcpy(payload, buf + AS_HEADER_LEN, hdr->payload_len);
        *payload_len = hdr->payload_len;
    } else {
        *payload_len = 0;
    }

    return 0;
}