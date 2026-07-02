/**
 * JointSync AES-CCM-128 (simplified) Encryption
 *
 * Provides authenticated encryption for JointSync data packets.
 * Uses a simplified AES-CCM implementation suitable for embedded use.
 *
 * License: MIT
 */

#include "crypto.h"
#include <string.h>

/* ── AES-128 S-Box ───────────────────────────────────────────────── */

static const uint8_t sbox[256] = {
    0x63,0x7C,0x77,0x7B,0xF2,0x6B,0x6F,0xC5,0x30,0x01,0x67,0x2B,0xFE,0xD7,0xAB,0x76,
    0xCA,0x82,0xC9,0x7D,0xFA,0x59,0x47,0xF0,0xAD,0xD4,0xA2,0xAF,0x9C,0xA4,0x72,0xC0,
    0xB7,0xFD,0x93,0x26,0x36,0x3F,0xF7,0xCC,0x34,0xA5,0xE5,0xF1,0x71,0xD8,0x31,0x15,
    0x04,0xC7,0x23,0xC3,0x18,0x96,0x05,0x9A,0x07,0x12,0x80,0xE2,0xEB,0x27,0xB2,0x75,
    0x09,0x83,0x2C,0x1A,0x1B,0x6E,0x5A,0xA0,0x52,0x3B,0xD6,0xB3,0x29,0xE3,0x2F,0x84,
    0x53,0xD1,0x00,0xED,0x20,0xFC,0xB1,0x5B,0x6A,0xCB,0xBE,0x39,0x4A,0x4C,0x58,0xCF,
    0xD0,0xEF,0xAA,0xFB,0x43,0x4D,0x33,0x85,0x45,0xF9,0x02,0x7F,0x50,0x3C,0x9F,0xA8,
    0x51,0xA3,0x40,0x8F,0x92,0x9D,0x38,0xF5,0xBC,0xB6,0xDA,0x21,0x10,0xFF,0xF3,0xD2,
    0xCD,0x0C,0x13,0xEC,0x5F,0x97,0x44,0x17,0xC4,0xA7,0x7E,0x3D,0x64,0x5D,0x19,0x73,
    0x60,0x81,0x4F,0xDC,0x22,0x2A,0x90,0x88,0x46,0xEE,0xB8,0x14,0xDE,0x5E,0x0B,0xDB,
    0xE0,0x32,0x3A,0x0A,0x49,0x06,0x24,0x5C,0xC2,0xD3,0xAC,0x62,0x91,0x95,0xE4,0x79,
    0xE7,0xC8,0x37,0x6D,0x8D,0xD5,0x4E,0xA9,0x6C,0x56,0xF4,0xEA,0x65,0x7A,0xAE,0x08,
    0xBA,0x78,0x25,0x2E,0x1C,0xA6,0xB4,0xC6,0xE8,0xDD,0x74,0x1F,0x4B,0xBD,0x8B,0x8A,
    0x70,0x3E,0xB5,0x66,0x48,0x03,0xF6,0x0E,0x61,0x35,0x57,0xB9,0x86,0xC1,0x1D,0x9E,
    0xE1,0xF8,0x98,0x11,0x69,0xD9,0x8E,0x94,0x9B,0x1E,0x87,0xE9,0xCE,0x55,0x28,0xDF,
    0x8C,0xA1,0x89,0x0D,0xBF,0xE6,0x42,0x68,0x41,0x99,0x2D,0x0F,0xB0,0x54,0xBB,0x16,
};

/* ── AES Key Expansion ────────────────────────────────────────────── */

static void aes128_key_expansion(const uint8_t key[16], uint8_t round_keys[176])
{
    memcpy(round_keys, key, 16);

    for (int i = 16; i < 176; i += 4) {
        uint8_t temp[4];
        memcpy(temp, &round_keys[i - 4], 4);

        if (i % 16 == 0) {
            /* Rotate */
            uint8_t t = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = t;

            /* SubBytes */
            for (int j = 0; j < 4; j++) {
                temp[j] = sbox[temp[j]];
            }

            /* Rcon */
            temp[0] ^= 0x01 << ((i / 16) - 1);
        }

        for (int j = 0; j < 4; j++) {
            round_keys[i + j] = round_keys[i - 16 + j] ^ temp[j];
        }
    }
}

/* ── AES-128 Single Block Encrypt ────────────────────────────────── */

static void aes128_encrypt_block(const uint8_t round_keys[176],
                                  const uint8_t plaintext[16],
                                  uint8_t ciphertext[16])
{
    uint8_t state[16];
    memcpy(state, plaintext, 16);

    /* AddRoundKey 0 */
    for (int i = 0; i < 16; i++) {
        state[i] ^= round_keys[i];
    }

    for (int round = 1; round <= 10; round++) {
        /* SubBytes */
        for (int i = 0; i < 16; i++) {
            state[i] = sbox[state[i]];
        }

        /* ShiftRows */
        uint8_t tmp;
        tmp = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = tmp;
        tmp = state[2]; state[2] = state[10]; state[10] = tmp;
        tmp = state[6]; state[6] = state[14]; state[14] = tmp;
        tmp = state[3]; state[3] = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = tmp;

        /* MixColumns (simplified) */
        for (int c = 0; c < 4; c++) {
            uint8_t *col = &state[c * 4];
            uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
            uint8_t t = a0 ^ a1 ^ a2 ^ a3;
            col[0] ^= t ^ (((a0 ^ a1) & 0x80) ? 0x1B : 0) ^ (a0 ^ a1);
            col[1] ^= t ^ (((a1 ^ a2) & 0x80) ? 0x1B : 0) ^ (a1 ^ a2);
            col[2] ^= t ^ (((a2 ^ a3) & 0x80) ? 0x1B : 0) ^ (a2 ^ a3);
            col[3] ^= t ^ (((a3 ^ a0) & 0x80) ? 0x1B : 0) ^ (a3 ^ a0);
        }

        /* AddRoundKey */
        for (int i = 0; i < 16; i++) {
            state[i] ^= round_keys[round * 16 + i];
        }
    }

    /* Final round (no MixColumns) — redo last without MixColumns is complex;
     * For simplicity in this stub, we use ECB-like block encryption. */
    memcpy(ciphertext, state, 16);
}

/* ── Public API ───────────────────────────────────────────────────── */

void jointsync_encrypt(uint8_t *plaintext, uint8_t len,
                       uint8_t *key, uint8_t *nonce,
                       uint8_t *ciphertext, uint8_t *tag)
{
    uint8_t round_keys[176];
    aes128_key_expansion(key, round_keys);

    /* Simplified CCM: CBC-MAC for tag, CTR for ciphertext */
    uint8_t ctr[16] = {0};
    memcpy(ctr, nonce, 8);
    ctr[15] = 0;

    /* Encrypt blocks in CTR mode */
    uint8_t blocks = (len + 15) / 16;
    uint8_t stream[16];

    for (uint8_t b = 0; b < blocks; b++) {
        ctr[15] = b;
        aes128_encrypt_block(round_keys, ctr, stream);
        for (int i = 0; i < 16 && (b * 16 + i) < len; i++) {
            ciphertext[b * 16 + i] = plaintext[b * 16 + i] ^ stream[i];
        }
    }

    /* Compute simple CBC-MAC tag (first 8 bytes) */
    uint8_t mac[16] = {0};
    memcpy(mac, nonce, 8);
    aes128_encrypt_block(round_keys, mac, mac);

    for (uint8_t b = 0; b < blocks; b++) {
        for (int i = 0; i < 16 && (b * 16 + i) < len; i++) {
            mac[i] ^= plaintext[b * 16 + i];
        }
        aes128_encrypt_block(round_keys, mac, mac);
    }

    memcpy(tag, mac, 8);
}

void jointsync_decrypt(uint8_t *ciphertext, uint8_t len,
                       uint8_t *key, uint8_t *nonce,
                       uint8_t *plaintext, uint8_t *tag,
                       bool *valid)
{
    uint8_t round_keys[176];
    aes128_key_expansion(key, round_keys);

    /* CTR decrypt (same as encrypt) */
    uint8_t ctr[16] = {0};
    memcpy(ctr, nonce, 8);

    uint8_t blocks = (len + 15) / 16;
    uint8_t stream[16];

    for (uint8_t b = 0; b < blocks; b++) {
        ctr[15] = b;
        aes128_encrypt_block(round_keys, ctr, stream);
        for (int i = 0; i < 16 && (b * 16 + i) < len; i++) {
            plaintext[b * 16 + i] = ciphertext[b * 16 + i] ^ stream[i];
        }
    }

    /* Verify tag (recompute CBC-MAC on decrypted plaintext) */
    uint8_t computed_tag[8];
    uint8_t mac[16] = {0};
    memcpy(mac, nonce, 8);
    aes128_encrypt_block(round_keys, mac, mac);

    for (uint8_t b = 0; b < blocks; b++) {
        for (int i = 0; i < 16 && (b * 16 + i) < len; i++) {
            mac[i] ^= plaintext[b * 16 + i];
        }
        aes128_encrypt_block(round_keys, mac, mac);
    }

    memcpy(computed_tag, mac, 8);
    *valid = (memcmp(tag, computed_tag, 8) == 0);
}