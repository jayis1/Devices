#ifndef GLUCOSYNC_CRYPTO_H
#define GLUCOSYNC_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>

/**
 * AES-128-CTR encryption for health data payloads.
 * Per-session keys derived from ECDH key exchange during pairing.
 */

void crypto_init(void);

/**
 * Encrypt/decrypt payload in-place using AES-128-CTR.
 * key: 16 bytes, iv: 16 bytes (nonce + counter).
 * out and in may point to the same buffer.
 */
void crypto_aes128_ctr(const uint8_t *key, const uint8_t *iv,
                       const uint8_t *in, uint8_t *out, uint16_t len);

/**
 * Generate ECDH key pair for pairing (P-256).
 * Returns public key (65 bytes uncompressed) in pub_out,
 * private key (32 bytes) in priv_out.
 */
void crypto_ecdh_keygen(uint8_t *pub_out, uint8_t *priv_out);

/**
 * Derive shared session key from remote public key + local private key.
 * Returns 16-byte AES key.
 */
void crypto_ecdh_derive(const uint8_t *remote_pub, const uint8_t *local_priv,
                        uint8_t *aes_key_out);

#endif