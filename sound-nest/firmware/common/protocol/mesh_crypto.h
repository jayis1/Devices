/**
 * @file mesh_crypto.h
 * @brief SoundNest mesh network AES-128-CCM encryption/decryption.
 *
 * Provides authenticated encryption for all Sub-GHz mesh packets
 * using AES-128 in CCM (Counter with CBC-MAC) mode.
 */

#ifndef MESH_CRYPTO_H
#define MESH_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* AES-128 block size */
#define AES_BLOCK_SIZE 16

/* CCM parameters */
#define CCM_NONCE_LEN    13
#define CCM_MIC_LEN       4   /* 32-bit MIC */
#define CCM_MAC_LEN       8   /* Full CBC-MAC tag (truncated to 4) */

/**
 * Initialize the crypto module with the network key.
 * @param key 16-byte AES-128 network key
 * @return 0 on success, negative on error
 */
int mesh_crypto_init(const uint8_t key[16]);

/**
 * Encrypt and authenticate a mesh packet payload.
 * Produces encrypted payload + 4-byte MIC.
 *
 * @param src_addr  Source node address (used in nonce)
 * @param seq_num   Sequence number (used in nonce)
 * @param msg_type  Message type (used in associated data)
 * @param plaintext Input payload data
 * @param pt_len    Length of plaintext
 * @param ciphertext Output buffer (must be pt_len + 4 bytes)
 * @return 0 on success, negative on error
 */
int mesh_crypto_encrypt(uint16_t src_addr, uint16_t seq_num,
                         uint8_t msg_type,
                         const uint8_t *plaintext, uint8_t pt_len,
                         uint8_t *ciphertext);

/**
 * Decrypt and verify a mesh packet payload.
 * Checks MIC and decrypts payload.
 *
 * @param src_addr   Source node address (used in nonce)
 * @param seq_num    Sequence number (used in nonce)
 * @param msg_type   Message type (used in associated data)
 * @param ciphertext Input ciphertext + MIC
 * @param ct_len     Length of ciphertext (including 4-byte MIC)
 * @param plaintext  Output buffer (must be ct_len - 4 bytes)
 * @return 0 on success (MIC valid), negative on error or MIC failure
 */
int mesh_crypto_decrypt(uint16_t src_addr, uint16_t seq_num,
                         uint8_t msg_type,
                         const uint8_t *ciphertext, uint8_t ct_len,
                         uint8_t *plaintext);

/**
 * Generate a session key from the network key and node address.
 * Used during node provisioning for per-node encryption keys.
 *
 * @param network_key Master network key (16 bytes)
 * @param node_addr   Node address
 * @param session_key  Output session key (16 bytes)
 */
void mesh_crypto_derive_session_key(const uint8_t network_key[16],
                                    uint16_t node_addr,
                                    uint8_t session_key[16]);

/**
 * Generate a random nonce for CCM encryption.
 *
 * @param src_addr Source address
 * @param seq_num  Sequence number
 * @param nonce    Output 13-byte nonce
 */
void mesh_crypto_generate_nonce(uint16_t src_addr, uint16_t seq_num,
                                 uint8_t nonce[CCM_NONCE_LEN]);

#endif /* MESH_CRYPTO_H */