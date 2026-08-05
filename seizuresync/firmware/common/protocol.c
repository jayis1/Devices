/*
 * SeizureSync — Shared Protocol C implementation
 * Crypto + TDMA timing helpers.
 *
 * SPDX-License-Identifier: MIT
 */
#include "protocol.h"

/* ---- AES-128 CTR mode (simplified, no external dep) ----
 * For production use a hardware AES engine (ESP32-S3 / nRF52840 have one).
 * This is a portable reference that works everywhere. */

static const uint8_t sbox[256] = {
    /* ... S-box values omitted for brevity in stub; use mbedTLS or
     * hardware AES in production. ... */
    0
};

/* Placeholder: in real firmware, call esp_aes / nrf_crypto HW engines. */
void sz_crypto_encrypt_ctr(const uint8_t *key, const uint8_t *iv,
                           const uint8_t *in, uint8_t *out, size_t len)
{
    /* TODO: delegate to hardware AES-CTR.
     * ESP32-S3: esp_aes_crypt_ctr()
     * nRF52840: nrf_ecb_crypt() in CTR mode (soft impl)
     * For now, XOR with keystream derived from key+iv (PLACEHOLDER ONLY). */
    uint8_t ks[16];
    /* Derive a simple keystream (NOT cryptographically secure — stub). */
    for (int i = 0; i < 16; i++) ks[i] = key[i] ^ iv[i];
    for (size_t i = 0; i < len; i++)
        out[i] = in[i] ^ ks[i % 16];
}

/* ---- TDMA timing ----
 * Hub broadcasts BEACON at start of each superframe (slot 0).
 * Nodes synchronize to beacon and transmit only in their assigned slot. */

void sz_tdma_get_slot_timing(uint8_t my_slot, uint32_t *offset_ms,
                              uint32_t *duration_ms)
{
    *offset_ms   = my_slot * SZ_TDMA_SLOT_MS;
    *duration_ms = SZ_TDMA_SLOT_MS - 5;   /* 5 ms guard */
}

/* ---- Packet parsing ---- */
int sz_parse(const uint8_t *buf, size_t len, sz_header_t *out_h,
             uint8_t *out_payload, size_t *out_plen)
{
    if (len < SZ_HEADER_LEN + 2) return -1;
    memcpy(out_h, buf, SZ_HEADER_LEN);
    size_t plen = len - SZ_HEADER_LEN - 2;
    if (out_payload && out_plen) {
        memcpy(out_payload, buf + SZ_HEADER_LEN, plen);
        /* verify CRC */
        uint16_t crc_recv = buf[SZ_HEADER_LEN + plen]
                          | (buf[SZ_HEADER_LEN + plen + 1] << 8);
        uint16_t crc_calc = sz_crc16(out_payload, plen);
        if (crc_recv != crc_calc) return -2;
        *out_plen = plen;
    }
    return 0;
}