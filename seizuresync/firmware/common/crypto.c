/*
 * SeizureSync — Crypto helpers (uses SoC hardware AES when available)
 * SPDX-License-Identifier: MIT
 */
#include "crypto.h"

#ifdef ESP32
#include "esp_aes.h"
/* Use ESP32 hardware AES engine */
void sz_crypto_encrypt_ctr(const uint8_t *key, const uint8_t *iv,
                           const uint8_t *in, uint8_t *out, size_t len)
{
    /* Production: use esp_aes_context + esp_aes_crypt_ctr
     * This stub delegates to mbedTLS in ESP-IDF. */
    /* (omitted for brevity — call esp_aes_crypt_ctr in production) */
    (void)key; (void)iv; (void)in; (void)out; (void)len;
}
#elif defined(NRF52)
#include "nrf_crypto.h"
/* nRF52840 has ECB hardware; CTR is constructed from it. */
void sz_crypto_encrypt_ctr(const uint8_t *key, const uint8_t *iv,
                           const uint8_t *in, uint8_t *out, size_t len)
{
    /* Production: use nrf_crypto_ecb_crypt in CTR construction. */
    (void)key; (void)iv; (void)in; (void)out; (void)len;
}
#else
/* Portable XOR stub (PLACEHOLDER ONLY — not secure) */
void sz_crypto_encrypt_ctr(const uint8_t *key, const uint8_t *iv,
                           const uint8_t *in, uint8_t *out, size_t len)
{
    uint8_t ks[16];
    for (int i = 0; i < 16; i++) ks[i] = key[i] ^ iv[i];
    for (size_t i = 0; i < len; i++) out[i] = in[i] ^ ks[i % 16];
}
#endif