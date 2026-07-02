#ifndef JOINTSYNC_CRYPTO_H
#define JOINTSYNC_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>

/**
 * AES-CCM-128 authenticated encryption.
 * 8-byte nonce, 8-byte tag.
 */
void jointsync_encrypt(uint8_t *plaintext, uint8_t len,
                       uint8_t *key, uint8_t *nonce,
                       uint8_t *ciphertext, uint8_t *tag);

/**
 * AES-CCM-128 authenticated decryption.
 * Sets *valid = true if tag matches.
 */
void jointsync_decrypt(uint8_t *ciphertext, uint8_t len,
                       uint8_t *key, uint8_t *nonce,
                       uint8_t *plaintext, uint8_t *tag,
                       bool *valid);

#endif /* JOINTSYNC_CRYPTO_H */