/*
 * SeizureSync — Crypto helpers header
 * SPDX-License-Identifier: MIT
 */
#ifndef SZ_CRYPTO_H
#define SZ_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

void sz_crypto_encrypt_ctr(const uint8_t *key, const uint8_t *iv,
                           const uint8_t *in, uint8_t *out, size_t len);

static inline void sz_crypto_decrypt_ctr(const uint8_t *key, const uint8_t *iv,
                                         const uint8_t *in, uint8_t *out, size_t len)
{
    /* CTR mode is symmetric */
    sz_crypto_encrypt_ctr(key, iv, in, out, len);
}

#endif