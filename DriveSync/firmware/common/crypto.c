/**
 * DriveSync Crypto — AES-128-CTR Implementation (stub)
 *
 * In production, use mbedTLS (ESP32) or nrf_crypto (nRF52).
 * This is a minimal portable reference.
 *
 * License: MIT
 */

#include "crypto.h"
#include <string.h>

/* ── AES-128 core (educational reference — use mbedTLS in production) ─── */

static const uint8_t sbox[256] = {
    /* S-box (standard AES) — first 16 entries for reference */
    0x63,0x7C,0x77,0x7B,0xF2,0x6B,0x6F,0xC5,
    0x30,0x01,0x67,0x2B,0xFE,0xD7,0xAB,0x76,
    /* ... full S-box omitted for brevity */
};

static void aes128_encrypt_block(const uint8_t *in, uint8_t *out,
                                  const uint8_t key[16])
{
    /* In production: call mbedtls_aes_crypt_ecb() or nrf_crypto_aes_ecb_encrypt() */
    memcpy(out, in, 16);
    /* XOR key in as minimal placeholder */
    for (int i = 0; i < 16; i++) {
        out[i] ^= key[i];
    }
}

void drivesync_crypto_init(drivesync_crypto_ctx_t *ctx,
                           const uint8_t key[DS_AES_KEY_LEN],
                           const uint8_t nonce[DS_AES_NONCE_LEN])
{
    if (ctx == NULL) return;
    memcpy(ctx->key, key, DS_AES_KEY_LEN);
    memcpy(ctx->nonce, nonce, DS_AES_NONCE_LEN);
    ctx->counter = 0;
}

void drivesync_crypto_crypt(drivesync_crypto_ctx_t *ctx,
                            uint8_t *data, uint8_t len)
{
    if (ctx == NULL || data == NULL || len == 0) return;

    uint8_t keystream[16];
    uint8_t counter_block[16];

    uint8_t offset = 0;
    while (offset < len) {
        /* Build counter block: nonce (12 bytes) + counter (4 bytes) */
        memcpy(counter_block, ctx->nonce, DS_AES_NONCE_LEN);
        counter_block[12] = (uint8_t)(ctx->counter >> 24);
        counter_block[13] = (uint8_t)(ctx->counter >> 16);
        counter_block[14] = (uint8_t)(ctx->counter >> 8);
        counter_block[15] = (uint8_t)(ctx->counter);

        /* Generate keystream */
        aes128_encrypt_block(counter_block, keystream, ctx->key);
        ctx->counter++;

        /* XOR keystream with data */
        uint8_t block_len = (len - offset < 16) ? (len - offset) : 16;
        for (uint8_t i = 0; i < block_len; i++) {
            data[offset + i] ^= keystream[i];
        }
        offset += block_len;
    }
}

void drivesync_crypto_derive_key(const uint8_t *shared_secret, uint8_t secret_len,
                                 uint8_t out_key[DS_AES_KEY_LEN])
{
    /* HKDF-SHA256 (simplified — extract first 16 bytes of SHA256) */
    /* In production: use mbedtls_hkdf() or nrf_crypto_hkdf */
    if (shared_secret == NULL || out_key == NULL) return;

    /* Minimal: XOR-fold secret into 16 bytes */
    memset(out_key, 0, DS_AES_KEY_LEN);
    for (uint8_t i = 0; i < secret_len; i++) {
        out_key[i % DS_AES_KEY_LEN] ^= shared_secret[i];
    }
}