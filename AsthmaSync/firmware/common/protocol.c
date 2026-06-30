/**
 * AsthmaSync — Protocol Implementation
 * Packing / unpacking / CRC
 *
 * License: MIT
 */

#include "protocol.h"
#include <string.h>

/* ── CRC-16/CCITT (poly 0x1021, init 0xFFFF) ──────────── */
static uint16_t crc_table[256];
static int crc_table_ready = 0;

static void crc_init(void)
{
    for (uint16_t i = 0; i < 256; i++) {
        uint16_t crc = i << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
        crc_table[i] = crc;
    }
    crc_table_ready = 1;
}

uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    if (!crc_table_ready)
        crc_init();

    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
        crc = (crc << 8) ^ crc_table[(crc >> 8) ^ data[i]];
    return crc;
}

/* ── Pack ───────────────────────────────────────────────── */
size_t proto_pack(pkt_header_t *hdr, const uint8_t *payload,
                  uint16_t payload_len, uint8_t *out_buf, size_t buf_size)
{
    if (!hdr || !out_buf)
        return 0;
    if (payload_len > PKT_MAX_PAYLOAD)
        return 0;
    if (buf_size < (size_t)(PKT_HEADER_SIZE + payload_len))
        return 0;

    hdr->magic       = PKT_MAGIC;
    hdr->version     = PROTO_VERSION;
    hdr->payload_len = payload_len;
    hdr->crc         = 0;  /* zero for CRC computation */

    /* Copy header to buffer */
    memcpy(out_buf, hdr, PKT_HEADER_SIZE);

    /* Copy payload */
    if (payload && payload_len > 0)
        memcpy(out_buf + PKT_HEADER_SIZE, payload, payload_len);

    /* Compute CRC over everything except the last 2 bytes (crc field) */
    size_t total = PKT_HEADER_SIZE + payload_len;
    uint16_t crc = crc16_ccitt(out_buf, total - 2);

    /* Write CRC into header in buffer */
    pkt_header_t *buf_hdr = (pkt_header_t *)out_buf;
    buf_hdr->crc = crc;
    hdr->crc = crc;

    return total;
}

/* ── Unpack ─────────────────────────────────────────────── */
int proto_unpack(const uint8_t *buf, size_t buf_len,
                 pkt_header_t *out_hdr, uint8_t *out_payload,
                 uint16_t *out_payload_len)
{
    if (!buf || !out_hdr)
        return -1;
    if (buf_len < PKT_HEADER_SIZE)
        return -2;

    /* Copy header */
    memcpy(out_hdr, buf, PKT_HEADER_SIZE);

    /* Validate magic */
    if (out_hdr->magic != PKT_MAGIC)
        return -3;

    /* Validate version */
    if ((out_hdr->version >> 4) != PROTO_VERSION_MAJOR)
        return -4;

    /* Validate payload length */
    if (out_hdr->payload_len > PKT_MAX_PAYLOAD)
        return -5;

    size_t total = PKT_HEADER_SIZE + out_hdr->payload_len;
    if (buf_len < total)
        return -6;

    /* Verify CRC */
    uint16_t expected = out_hdr->crc;
    uint16_t computed = crc16_ccitt(buf, total - 2);
    if (expected != computed)
        return -7;

    /* Copy payload */
    if (out_payload && out_payload_len) {
        if (out_hdr->payload_len > 0)
            memcpy(out_payload, buf + PKT_HEADER_SIZE, out_hdr->payload_len);
        *out_payload_len = out_hdr->payload_len;
    }

    return 0;
}