/**
 * @file psp_protocol.c
 * @brief PoolSync Protocol (PSP) implementation — encode, decode, CRC, AES
 */

#include "psp_protocol.h"
#include <string.h>

/* ============================================================
 * CRC16-CCITT (0x1021 polynomial)
 * ============================================================ */

uint16_t psp_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* ============================================================
 * FRAME ENCODE
 * ============================================================ */

uint16_t psp_encode(const psp_header_t *header, const uint8_t *payload,
                    uint16_t payload_len, uint8_t *out_buf, uint16_t out_buf_size)
{
    if (!header || !out_buf)
        return 0;
    if (payload_len > PSP_MAX_PAYLOAD)
        return 0;

    uint16_t total_len = PSP_HEADER_SIZE + payload_len + PSP_CRC_SIZE;
    if (total_len > out_buf_size)
        return 0;

    uint16_t idx = 0;

    /* Header */
    out_buf[idx++] = (header->preamble >> 8) & 0xFF;
    out_buf[idx++] = (header->preamble) & 0xFF;
    out_buf[idx++] = (header->sync_word >> 8) & 0xFF;
    out_buf[idx++] = (header->sync_word) & 0xFF;
    out_buf[idx++] = (total_len >> 8) & 0xFF;
    out_buf[idx++] = (total_len) & 0xFF;
    out_buf[idx++] = (header->src_addr >> 8) & 0xFF;
    out_buf[idx++] = (header->src_addr) & 0xFF;
    out_buf[idx++] = (header->dst_addr >> 8) & 0xFF;
    out_buf[idx++] = (header->dst_addr) & 0xFF;
    out_buf[idx++] = header->msg_type;

    /* Payload */
    if (payload && payload_len > 0) {
        memcpy(&out_buf[idx], payload, payload_len);
        idx += payload_len;
    }

    /* CRC over header + payload */
    uint16_t crc = psp_crc16(out_buf, idx);
    out_buf[idx++] = (crc >> 8) & 0xFF;
    out_buf[idx++] = (crc) & 0xFF;

    return idx;
}

/* ============================================================
 * FRAME DECODE
 * ============================================================ */

int psp_decode(const uint8_t *raw, uint16_t raw_len, psp_frame_t *frame)
{
    if (!raw || !frame || raw_len < PSP_HEADER_SIZE + PSP_CRC_SIZE)
        return -1;

    uint16_t idx = 0;

    /* Parse header */
    frame->header.preamble  = (raw[idx] << 8) | raw[idx + 1]; idx += 2;
    frame->header.sync_word = (raw[idx] << 8) | raw[idx + 1]; idx += 2;
    frame->header.length    = (raw[idx] << 8) | raw[idx + 1]; idx += 2;
    frame->header.src_addr  = (raw[idx] << 8) | raw[idx + 1]; idx += 2;
    frame->header.dst_addr  = (raw[idx] << 8) | raw[idx + 1]; idx += 2;
    frame->header.msg_type  = raw[idx++];

    /* Validate preamble and sync */
    if (frame->header.preamble != PSP_PREAMBLE)
        return -2;
    if (frame->header.sync_word != PSP_SYNC_WORD)
        return -3;
    if (frame->header.length != raw_len)
        return -4;

    /* Extract payload */
    uint16_t payload_len = raw_len - PSP_HEADER_SIZE - PSP_CRC_SIZE;
    if (payload_len > PSP_MAX_PAYLOAD)
        return -5;

    if (payload_len > 0) {
        memcpy(frame->payload, &raw[PSP_HEADER_SIZE], payload_len);
    }
    frame->payload_len = payload_len;

    /* Verify CRC */
    uint16_t received_crc = (raw[raw_len - 2] << 8) | raw[raw_len - 1];
    uint16_t computed_crc = psp_crc16(raw, raw_len - 2);
    if (received_crc != computed_crc)
        return -6;

    frame->crc = received_crc;
    return 0;
}

/* ============================================================
 * AES-128-GCM (STUB — use hardware crypto or mbedTLS)
 * ============================================================ */

static uint8_t psp_aes_key[PSP_AES_KEY_SIZE] = {0};
static uint16_t psp_self_addr = 0;

void psp_set_key(const uint8_t key[PSP_AES_KEY_SIZE])
{
    memcpy(psp_aes_key, key, PSP_AES_KEY_SIZE);
}

uint16_t psp_get_self_addr(void)
{
    return psp_self_addr;
}

int psp_encrypt(const uint8_t *key, const uint8_t *nonce,
                const uint8_t *plaintext, uint16_t pt_len,
                uint8_t *ciphertext, uint8_t *tag)
{
    /* In production: use STM32 AES hardware (CRYP peripheral) or mbedTLS */
    /* For now: pass-through (encryption disabled in development) */
    (void)key;
    (void)nonce;
    memcpy(ciphertext, plaintext, pt_len);
    memset(tag, 0, PSP_AES_TAG_SIZE);
    return 0;
}

int psp_decrypt(const uint8_t *key, const uint8_t *nonce,
                const uint8_t *ciphertext, uint16_t ct_len,
                const uint8_t *tag, uint8_t *plaintext)
{
    /* In production: use STM32 AES hardware or mbedTLS with tag verification */
    (void)key;
    (void)nonce;
    (void)tag;
    memcpy(plaintext, ciphertext, ct_len);
    return 0;
}

/* ============================================================
 * DEBUG PRINT
 * ============================================================ */

void psp_print_frame(const psp_frame_t *frame)
{
    /* Minimal debug output — override with platform-specific printf */
    (void)frame;
    /* Example:
    printf("PSP Frame: src=0x%04X dst=0x%04X type=0x%02X len=%u CRC=0x%04X\r\n",
           frame->header.src_addr, frame->header.dst_addr,
           frame->header.msg_type, frame->payload_len, frame->crc);
    */
}