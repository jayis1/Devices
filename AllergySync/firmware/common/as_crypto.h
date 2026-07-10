/*
 * AllergySync — Shared Crypto Interface
 * AES-128-CCM encryption/decryption wrapper.
 * Each platform implements these with its own crypto library:
 *   - ESP32-S3: mbedTLS (hardware AES accelerator)
 *   - nRF52840: nRF Crypto (CC310)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef AS_CRYPTO_H
#define AS_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

/*
 * AES-CCM-128 encrypt + authenticate.
 *   key:     16-byte AES key
 *   nonce:   13-byte nonce (TDMA slot + frame counter + src_id)
 *   plaintext: input data
 *   pt_len:  plaintext length
 *   aad:     additional authenticated data (header), may be NULL
 *   aad_len: AAD length
 *   ciphertext: output (pt_len bytes)
 *   mic:     4-byte output authentication tag
 * Returns 0 on success.
 */
int as_ccm_encrypt(const uint8_t *key, const uint8_t *nonce,
                   const uint8_t *plaintext, size_t pt_len,
                   const uint8_t *aad, size_t aad_len,
                   uint8_t *ciphertext, uint8_t *mic);

/*
 * AES-CCM-128 decrypt + verify.
 * Returns 0 on success (MIC valid), -1 on failure.
 */
int as_ccm_decrypt(const uint8_t *key, const uint8_t *nonce,
                   const uint8_t *ciphertext, size_t ct_len,
                   const uint8_t *aad, size_t aad_len,
                   uint8_t *plaintext, const uint8_t *mic);

/*
 * ECDH P-256 key exchange.
 *   pubkey_out: 64-byte public key (X || Y, big-endian)
 *   privkey_out: 32-byte private key (store securely)
 * Returns 0 on success.
 */
int as_ecdh_p256_generate(uint8_t *pubkey_out, uint8_t *privkey_out);

/*
 * ECDH P-256 shared secret derivation.
 *   peer_pubkey: 64-byte peer public key
 *   privkey:     32-byte our private key
 *   shared_out:  32-byte shared secret
 * Returns 0 on success.
 */
int as_ecdh_p256_shared(const uint8_t *peer_pubkey, const uint8_t *privkey,
                        uint8_t *shared_out);

/*
 * HKDF-SHA256 to derive session key from shared secret.
 *   shared_secret: 32 bytes from ECDH
 *   info:          context string
 *   key_out:       16-byte AES-128 key
 */
void as_hkdf_aes128(const uint8_t *shared_secret, const char *info,
                    uint8_t *key_out);

#endif /* AS_CRYPTO_H */