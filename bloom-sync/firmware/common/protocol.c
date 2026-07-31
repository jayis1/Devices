/*
 * BloomSync — Protocol Implementation
 * Binary message encoding/decoding + CRC-16-CCITT + AES-128-CTR stub.
 */
#include "protocol.h"

/* === CRC-16-CCITT (poly 0x1021, init 0xFFFF) === */
uint16_t bs_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ BS_CRC_POLY;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* === Message encode === */
size_t bs_encode(uint8_t *out, size_t out_cap,
                 uint8_t src_id, uint8_t dst_id,
                 uint8_t msg_type, uint8_t subtype,
                 uint8_t seq, const uint8_t *payload, size_t payload_len)
{
    if (!out || payload_len > BS_MAX_PAYLOAD)
        return 0;

    size_t total = 8 + payload_len + 2;  /* header + payload + crc */
    if (total > out_cap)
        return 0;

    /* Header */
    out[0] = BS_SYNC0;
    out[1] = BS_SYNC1;
    out[2] = src_id;
    out[3] = dst_id;
    out[4] = msg_type;
    out[5] = subtype;
    out[6] = seq;
    out[7] = (uint8_t)payload_len;

    /* Payload */
    if (payload_len > 0 && payload)
        memcpy(&out[8], payload, payload_len);

    /* CRC over header + payload */
    uint16_t crc = bs_crc16(out, 8 + payload_len);
    out[8 + payload_len]     = (uint8_t)(crc >> 8);
    out[8 + payload_len + 1] = (uint8_t)(crc & 0xFF);

    return total;
}

/* === Message decode === */
int bs_decode(const uint8_t *in, size_t in_len,
              bs_msg_header_t *hdr, uint8_t *payload, size_t payload_cap)
{
    if (!in || !hdr || in_len < 10)
        return -1;

    /* Check sync bytes */
    if (in[0] != BS_SYNC0 || in[1] != BS_SYNC1)
        return -1;

    /* Parse header */
    hdr->sync0       = in[0];
    hdr->sync1       = in[1];
    hdr->src_id      = in[2];
    hdr->dst_id      = in[3];
    hdr->msg_type    = in[4];
    hdr->subtype     = in[5];
    hdr->seq         = in[6];
    hdr->payload_len = in[7];

    size_t payload_len = hdr->payload_len;
    if (in_len < (size_t)(8 + payload_len + 2))
        return -1;

    /* Verify CRC */
    uint16_t crc_calc = bs_crc16(in, 8 + payload_len);
    uint16_t crc_recv = ((uint16_t)in[8 + payload_len] << 8) |
                         (uint16_t)in[8 + payload_len + 1];
    if (crc_calc != crc_recv)
        return -1;

    /* Copy payload */
    if (payload_len > 0) {
        if (!payload || payload_cap < payload_len)
            return -1;
        memcpy(payload, &in[8], payload_len);
    }

    return (int)payload_len;
}

/* === AES-128-CTR encrypt/decrypt (stub — use mbedTLS in production) ===
 * In production: use mbedtls_aes_crypt_ctr() from ESP-IDF / nRF SDK.
 * This stub XORs with a keystream placeholder for protocol completeness.
 */
void bs_aes_encrypt(uint8_t *data, size_t len, const uint8_t *key, const uint8_t *nonce)
{
    (void)key;
    (void)nonce;
    /* Production: generate AES-128-CTR keystream from key+nonce,
     * XOR with data. Same function used for decrypt (symmetric). */
    for (size_t i = 0; i < len; i++) {
        data[i] ^= (uint8_t)(i & 0xFF);  /* placeholder — NOT secure */
    }
}