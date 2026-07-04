/**
 * Crypto stubs — AES-128-CTR + ECDH key exchange.
 * In production, use mbedTLS (ESP-IDF) or nrf_crypto (nRF5 SDK).
 * License: MIT
 */

#include "crypto.h"
#include <string.h>

/* XOR-based placeholder. Production code MUST use mbedTLS aes_crypt_ctr. */

void crypto_init(void)
{
    /* Initialize RNG, etc. */
}

void crypto_aes128_ctr(const uint8_t *key, const uint8_t *iv,
                       const uint8_t *in, uint8_t *out, uint16_t len)
{
    /* Placeholder: XOR with key-derived keystream.
     * Production: mbedtls_aes_crypt_ctr() */
    for (uint16_t i = 0; i < len; i++) {
        uint8_t ks = key[i % 16] ^ iv[i % 16];
        out[i] = in[i] ^ ks;
    }
}

void crypto_ecdh_keygen(uint8_t *pub_out, uint8_t *priv_out)
{
    /* Placeholder: zero keys.
     * Production: mbedtls_ecdh_gen_public() with ECP group P-256 */
    memset(pub_out, 0, 65);
    memset(priv_out, 0, 32);
}

void crypto_ecdh_derive(const uint8_t *remote_pub, const uint8_t *local_priv,
                        uint8_t *aes_key_out)
{
    /* Placeholder: derive 16-byte key from XOR of inputs.
     * Production: mbedtls_ecdh_compute_shared() */
    for (int i = 0; i < 16; i++) {
        aes_key_out[i] = remote_pub[i] ^ local_priv[i];
    }
}