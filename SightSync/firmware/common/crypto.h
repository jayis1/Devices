#ifndef SIGHTSYNC_CRYPTO_H
#define SIGHTSYNC_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>

/* AES-128-CTR encryption for SightSync packets.
 * On ESP32-S3 uses hardware AES; on nRF52840 uses software (micro-ecc).
 * On RP2040 uses software (pico-aes). */

#define SS_AES_KEY_LEN   16
#define SS_AES_BLOCK_LEN  16

void sightsync_crypto_init(const uint8_t *key_16);
void sightsync_crypto_set_iv(const uint8_t *iv_16);

/* Encrypt/decrypt in place (CTR mode is symmetric). */
void sightsync_crypto_ctr(const uint8_t *input, uint8_t *output, uint8_t len);

#endif /* SIGHTSYNC_CRYPTO_H */