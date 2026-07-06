/**
 * SightSync AES-128-CTR Encryption — Software Implementation
 *
 * Portable C implementation of AES-128 for use on RP2040 and nRF52840.
 * On ESP32-S3, this is replaced by hardware AES (esp_aes).
 *
 * License: MIT
 */

#include "crypto.h"
#include <string.h>

static uint8_t s_key[SS_AES_KEY_LEN];
static uint8_t s_iv[SS_AES_BLOCK_LEN];
static uint8_t s_counter[SS_AES_BLOCK_LEN];
static bool s_init = false;

/* ── AES S-Box ───────────────────────────────────────────────────── */

static const uint8_t sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16,
};

/* ── AES Round Constants ─────────────────────────────────────────── */

static const uint8_t rcon[11] = {
    0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36
};

/* ── AES-128 Key Expansion ──────────────────────────────────────── */

static uint8_t round_keys[176]; /* 11 rounds × 16 bytes */

static void aes_key_expansion(const uint8_t *key)
{
    memcpy(round_keys, key, 16);

    for (uint8_t i = 1; i <= 10; i++) {
        uint8_t *prev = &round_keys[(i - 1) * 16];
        uint8_t *curr = &round_keys[i * 16];

        uint8_t tmp[4];
        tmp[0] = sbox[prev[13]] ^ rcon[i];
        tmp[1] = sbox[prev[14]];
        tmp[2] = sbox[prev[15]];
        tmp[3] = sbox[prev[12]];

        for (uint8_t j = 0; j < 4; j++) {
            curr[j]     = prev[j]     ^ tmp[j];
            curr[j + 4] = prev[j + 4] ^ curr[j];
            curr[j + 8] = prev[j + 8] ^ curr[j + 4];
            curr[j + 12]= prev[j + 12]^ curr[j + 8];
        }
    }
}

/* ── AES-128 Single Block Encrypt ───────────────────────────────── */

static void aes_encrypt_block(const uint8_t *in, uint8_t *out, const uint8_t *rk)
{
    uint8_t state[16];
    memcpy(state, in, 16);

    /* AddRoundKey (round 0) */
    for (uint8_t i = 0; i < 16; i++) state[i] ^= rk[i];

    for (uint8_t round = 1; round <= 10; round++) {
        const uint8_t *rk_round = &rk[round * 16];

        /* SubBytes */
        for (uint8_t i = 0; i < 16; i++) state[i] = sbox[state[i]];

        /* ShiftRows */
        uint8_t t;
        t = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = t;
        t = state[2]; state[2] = state[10]; state[10] = t;
        t = state[6]; state[6] = state[14]; state[14] = t;
        t = state[3]; state[3] = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = t;

        /* MixColumns (skip on round 10) */
        if (round < 10) {
            for (uint8_t c = 0; c < 4; c++) {
                uint8_t a0 = state[c * 4], a1 = state[c * 4 + 1],
                        a2 = state[c * 4 + 2], a3 = state[c * 4 + 3];
                uint8_t tmp = a0 ^ a1 ^ a2 ^ a3;
                state[c * 4]     = a0 ^ tmp ^ ((a0 ^ a1) & 0x80 ? (a0 ^ a1) << 1 ^ 0x1b : (a0 ^ a1) << 1);
                state[c * 4 + 1] = a1 ^ tmp ^ ((a1 ^ a2) & 0x80 ? (a1 ^ a2) << 1 ^ 0x1b : (a1 ^ a2) << 1);
                state[c * 4 + 2] = a2 ^ tmp ^ ((a2 ^ a3) & 0x80 ? (a2 ^ a3) << 1 ^ 0x1b : (a2 ^ a3) << 1);
                state[c * 4 + 3] = a3 ^ tmp ^ ((a3 ^ a0) & 0x80 ? (a3 ^ a0) << 1 ^ 0x1b : (a3 ^ a0) << 1);
            }
        }

        /* AddRoundKey */
        for (uint8_t i = 0; i < 16; i++) state[i] ^= rk_round[i];
    }

    memcpy(out, state, 16);
}

/* ── CTR Mode ───────────────────────────────────────────────────── */

void sightsync_crypto_init(const uint8_t *key_16)
{
    if (key_16 != NULL) {
        memcpy(s_key, key_16, SS_AES_KEY_LEN);
        aes_key_expansion(s_key);
    }
    s_init = true;
}

void sightsync_crypto_set_iv(const uint8_t *iv_16)
{
    if (iv_16 != NULL) {
        memcpy(s_iv, iv_16, SS_AES_BLOCK_LEN);
    }
    memcpy(s_counter, s_iv, SS_AES_BLOCK_LEN);
}

void sightsync_crypto_ctr(const uint8_t *input, uint8_t *output, uint8_t len)
{
    if (!s_init || input == NULL || output == NULL) {
        return;
    }

    uint8_t keystream[16];
    uint8_t offset = 0;

    while (offset < len) {
        aes_encrypt_block(s_counter, keystream, round_keys);

        uint8_t block_remaining = (len - offset < 16) ? (len - offset) : 16;
        for (uint8_t i = 0; i < block_remaining; i++) {
            output[offset + i] = input[offset + i] ^ keystream[i];
        }

        offset += block_remaining;

        /* Increment counter (big-endian, last 4 bytes) */
        for (int8_t i = 15; i >= 12; i--) {
            if (++s_counter[i] != 0) break;
        }
    }
}