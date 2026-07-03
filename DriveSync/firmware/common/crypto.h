#ifndef DRIVESYNC_CRYPTO_H
#define DRIVESYNC_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>

/**
 * AES-128-CTR encryption for DriveSync BLE payloads.
 * Session key is derived at pairing time using ECDH (P-256).
 * Only payloads with DS_FLAG_ENCRYPTED are encrypted.
 */

#define DS_AES_KEY_LEN    16
#define DS_AES_BLOCK_LEN  16
#define DS_AES_NONCE_LEN  12

typedef struct {
    uint8_t key[DS_AES_KEY_LEN];
    uint8_t nonce[DS_AES_NONCE_LEN];
    uint32_t counter;
} drivesync_crypto_ctx_t;

/**
 * Initialize crypto context with session key and nonce.
 */
void drivesync_crypto_init(drivesync_crypto_ctx_t *ctx,
                           const uint8_t key[DS_AES_KEY_LEN],
                           const uint8_t nonce[DS_AES_NONCE_LEN]);

/**
 * Encrypt/decrypt payload in-place (CTR mode is symmetric).
 * Length must be <= DS_MAX_PAYLOAD.
 */
void drivesync_crypto_crypt(drivesync_crypto_ctx_t *ctx,
                            uint8_t *data, uint8_t len);

/**
 * Derive session key from ECDH shared secret (HKDF-SHA256).
 */
void drivesync_crypto_derive_key(const uint8_t *shared_secret, uint8_t secret_len,
                                 uint8_t out_key[DS_AES_KEY_LEN]);

#endif /* DRIVESYNC_CRYPTO_H */