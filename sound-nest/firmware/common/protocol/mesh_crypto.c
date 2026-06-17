/**
 * @file mesh_crypto.c
 * @brief SoundNest mesh network AES-128-CCM encryption/decryption.
 *
 * Implements AES-128-CCM (Counter with CBC-MAC) for authenticated
 * encryption of mesh packets. Uses a software AES-128 implementation
 * suitable for embedded platforms (ESP32, nRF52).
 */

#include "mesh_crypto.h"

/* ── Software AES-128 (S-box based) ────────────────────────────────── */

static const uint8_t aes_sbox[256] = {
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
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x6f,0x0f,0x7e,0x0a,0x0b,0x0c,0x0d,0x0e,
    0x0f,0xf3,0x3e,0xd2,0x5d,0x6c,0x0a,0x47,0x8a,0xc4,0x18,0x76,0x0b,0x94,0x72,0x7e,
    0x43,0x9a,0xf2,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,0x60,0x81,
};

static const uint8_t aes_rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

/* AES-128 state: 4x4 bytes = 16 bytes */
typedef struct {
    uint8_t s[4][4];  /* state[row][col] */
} aes_state_t;

/* Module-level network key */
static uint8_t g_network_key[16];
static bool g_initialized = false;

/* ── AES-128 Core ───────────────────────────────────────────────────── */

static void aes_key_expansion(const uint8_t key[16], uint8_t round_keys[176])
{
    uint8_t temp[4];
    memcpy(round_keys, key, 16);

    for (int i = 4; i < 44; i++) {
        memcpy(temp, round_keys + (i - 1) * 4, 4);

        if (i % 4 == 0) {
            /* RotWord + SubWord + Rcon */
            uint8_t t = temp[0];
            temp[0] = aes_sbox[temp[1]] ^ aes_rcon[i / 4];
            temp[1] = aes_sbox[temp[2]];
            temp[2] = aes_sbox[temp[3]];
            temp[3] = aes_sbox[t];
        }

        for (int j = 0; j < 4; j++) {
            round_keys[i * 4 + j] = round_keys[(i - 4) * 4 + j] ^ temp[j];
        }
    }
}

static void aes_encrypt_block(const uint8_t round_keys[176],
                               const uint8_t input[16], uint8_t output[16])
{
    aes_state_t state;
    /* Column-major to row-major */
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            state.s[i][j] = input[j * 4 + i];

    /* AddRoundKey(0) */
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            state.s[i][j] ^= round_keys[(i * 4 + j)];

    /* Rounds 1-9 */
    for (int round = 1; round <= 9; round++) {
        /* SubBytes */
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                state.s[i][j] = aes_sbox[state.s[i][j]];

        /* ShiftRows */
        uint8_t t;
        t = state.s[1][0]; state.s[1][0] = state.s[1][1];
        state.s[1][1] = state.s[1][2]; state.s[1][2] = state.s[1][3];
        state.s[1][3] = t;

        t = state.s[2][0]; state.s[2][0] = state.s[2][2];
        state.s[2][2] = t;
        t = state.s[2][1]; state.s[2][1] = state.s[2][3];
        state.s[2][3] = t;

        t = state.s[3][3]; state.s[3][3] = state.s[3][2];
        state.s[3][2] = state.s[3][1]; state.s[3][1] = state.s[3][0];
        state.s[3][0] = t;

        /* MixColumns */
        for (int j = 0; j < 4; j++) {
            uint8_t a0 = state.s[0][j], a1 = state.s[1][j];
            uint8_t a2 = state.s[2][j], a3 = state.s[3][j];
            state.s[0][j] = (a0 << 1) ^ ((a1 << 1) ^ a1) ^ a2 ^ a3 ^
                             ((a0 << 7) >> 7) * 0x1b;
            state.s[1][j] = a0 ^ (a1 << 1) ^ ((a2 << 1) ^ a2) ^ a3 ^
                             ((a1 << 7) >> 7) * 0x1b;
            state.s[2][j] = a0 ^ a1 ^ (a2 << 1) ^ ((a3 << 1) ^ a3) ^
                             ((a2 << 7) >> 7) * 0x1b;
            state.s[3][j] = ((a0 << 1) ^ a0) ^ a1 ^ a2 ^ (a3 << 1) ^
                             ((a3 << 7) >> 7) * 0x1b;
        }

        /* AddRoundKey */
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                state.s[i][j] ^= round_keys[round * 16 + (i * 4 + j)];
    }

    /* Round 10 (SubBytes + ShiftRows + AddRoundKey, no MixColumns) */
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            state.s[i][j] = aes_sbox[state.s[i][j]];

    /* ShiftRows (same as above) */
    uint8_t t;
    t = state.s[1][0]; state.s[1][0] = state.s[1][1];
    state.s[1][1] = state.s[1][2]; state.s[1][2] = state.s[1][3];
    state.s[1][3] = t;

    t = state.s[2][0]; state.s[2][0] = state.s[2][2]; state.s[2][2] = t;
    t = state.s[2][1]; state.s[2][1] = state.s[2][3]; state.s[2][3] = t;

    t = state.s[3][3]; state.s[3][3] = state.s[3][2];
    state.s[3][2] = state.s[3][1]; state.s[3][1] = state.s[3][0]; state.s[3][0] = t;

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            state.s[i][j] ^= round_keys[160 + (i * 4 + j)];

    /* Row-major back to column-major output */
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            output[j * 4 + i] = state.s[i][j];
}

/* ── CCM Implementation ────────────────────────────────────────────── */

static void ccm_cbc_mac(const uint8_t key[16], const uint8_t *nonce,
                          const uint8_t *adata, size_t adata_len,
                          const uint8_t *payload, size_t payload_len,
                          uint8_t mac[8])
{
    uint8_t round_keys[176];
    aes_key_expansion(key, round_keys);

    uint8_t block[16];

    /* Block B0: Flags | Nonce | m_len */
    block[0] = 0x09;  /* L=2, M=4 (truncated to 8 then 4), adata present */
    memcpy(block + 1, nonce, CCM_NONCE_LEN);
    block[14] = (uint8_t)(payload_len >> 8);
    block[15] = (uint8_t)(payload_len & 0xFF);

    aes_encrypt_block(round_keys, block, block);

    /* Associated data */
    uint8_t adata_len_block[2] = {
        (uint8_t)(adata_len >> 8),
        (uint8_t)(adata_len & 0xFF)
    };

    /* XOR adata length */
    block[0] ^= adata_len_block[0];
    block[1] ^= adata_len_block[1];

    /* Process adata */
    size_t a_offset = 0;
    size_t remaining = adata_len;
    while (remaining > 0) {
        memset(block, 0, 16);
        size_t chunk = (remaining > 16) ? 16 : remaining;
        memcpy(block, adata + a_offset, chunk);
        /* XOR with previous MAC block */
        for (int i = 0; i < 16; i++) block[i] ^= mac[i];  /* first pass: mac is X0 */
        aes_encrypt_block(round_keys, block, mac);
        a_offset += chunk;
        remaining -= chunk;
    }

    /* Process payload */
    size_t p_offset = 0;
    remaining = payload_len;
    while (remaining > 0) {
        memset(block, 0, 16);
        size_t chunk = (remaining > 16) ? 16 : remaining;
        memcpy(block, payload + p_offset, chunk);
        for (int i = 0; i < 16; i++) block[i] ^= mac[i];
        aes_encrypt_block(round_keys, block, mac);
        p_offset += chunk;
        remaining -= chunk;
    }

    /* mac is 16 bytes but we only use first 8 */
}

static void ccm_ctr_encrypt(const uint8_t key[16], const uint8_t *nonce,
                              const uint8_t *input, size_t len,
                              uint8_t *output)
{
    uint8_t round_keys[176];
    aes_key_expansion(key, round_keys);

    uint8_t counter[16];
    /* Counter block: Flags | Nonce | Counter */
    counter[0] = 0x01;  /* L=2 */
    memcpy(counter + 1, nonce, CCM_NONCE_LEN);
    counter[14] = 0x00;
    counter[15] = 0x00;  /* Start at counter 0 for MAC encryption */

    size_t offset = 0;
    uint16_t ctr = 1;  /* Data starts at counter 1 */

    while (offset < len) {
        counter[14] = (uint8_t)(ctr >> 8);
        counter[15] = (uint8_t)(ctr & 0xFF);

        uint8_t keystream[16];
        aes_encrypt_block(round_keys, counter, keystream);

        size_t chunk = (len - offset > 16) ? 16 : (len - offset);
        for (size_t i = 0; i < chunk; i++) {
            output[offset + i] = input[offset + i] ^ keystream[i];
        }

        offset += chunk;
        ctr++;
    }
}

/* ── Public API ─────────────────────────────────────────────────────── */

int mesh_crypto_init(const uint8_t key[16])
{
    if (!key) return -1;
    memcpy(g_network_key, key, 16);
    g_initialized = true;
    return 0;
}

void mesh_crypto_generate_nonce(uint16_t src_addr, uint16_t seq_num,
                                 uint8_t nonce[CCM_NONCE_LEN])
{
    /* Nonce format: src_addr(2) | seq_num(2) | padding(9) */
    nonce[0] = (uint8_t)(src_addr >> 8);
    nonce[1] = (uint8_t)(src_addr & 0xFF);
    nonce[2] = (uint8_t)(seq_num >> 8);
    nonce[3] = (uint8_t)(seq_num & 0xFF);
    memset(nonce + 4, 0, CCM_NONCE_LEN - 4);  /* Zero padding */
}

int mesh_crypto_encrypt(uint16_t src_addr, uint16_t seq_num,
                         uint8_t msg_type,
                         const uint8_t *plaintext, uint8_t pt_len,
                         uint8_t *ciphertext)
{
    if (!g_initialized || !plaintext || !ciphertext) return -1;
    if (pt_len > MESH_MAX_PAYLOAD) return -2;

    uint8_t nonce[CCM_NONCE_LEN];
    mesh_crypto_generate_nonce(src_addr, seq_num, nonce);

    /* Associated data: message type byte */
    uint8_t adata = msg_type;

    /* Compute CBC-MAC */
    uint8_t mac[8];
    ccm_cbc_mac(g_network_key, nonce, &adata, 1,
                plaintext, pt_len, mac);

    /* CTR encrypt payload */
    ccm_ctr_encrypt(g_network_key, nonce, plaintext, pt_len, ciphertext);

    /* Append truncated MIC (4 bytes) */
    memcpy(ciphertext + pt_len, mac, CCM_MIC_LEN);

    return pt_len + CCM_MIC_LEN;
}

int mesh_crypto_decrypt(uint16_t src_addr, uint16_t seq_num,
                         uint8_t msg_type,
                         const uint8_t *ciphertext, uint8_t ct_len,
                         uint8_t *plaintext)
{
    if (!g_initialized || !ciphertext || !plaintext) return -1;
    if (ct_len < CCM_MIC_LEN) return -2;

    uint8_t pt_len = ct_len - CCM_MIC_LEN;
    if (pt_len > MESH_MAX_PAYLOAD) return -3;

    uint8_t nonce[CCM_NONCE_LEN];
    mesh_crypto_generate_nonce(src_addr, seq_num, nonce);

    /* CTR decrypt payload */
    ccm_ctr_encrypt(g_network_key, nonce, ciphertext, pt_len, plaintext);

    /* Verify MIC */
    uint8_t adata = msg_type;
    uint8_t mac[8];
    ccm_cbc_mac(g_network_key, nonce, &adata, 1,
                plaintext, pt_len, mac);

    if (memcmp(ciphertext + pt_len, mac, CCM_MIC_LEN) != 0) {
        return -4;  /* MIC verification failed */
    }

    return pt_len;
}

void mesh_crypto_derive_session_key(const uint8_t network_key[16],
                                    uint16_t node_addr,
                                    uint8_t session_key[16])
{
    /* Simple key derivation: AES-128 encrypt node_addr padded to 16 bytes */
    uint8_t input[16];
    uint8_t round_keys[176];

    memset(input, 0, 16);
    input[0] = (uint8_t)(node_addr >> 8);
    input[1] = (uint8_t)(node_addr & 0xFF);
    input[2] = 'S';  /* "SoundNest" */
    input[3] = 'N';
    input[4] = 'K';
    input[5] = 'D';  /* Key Derivation */

    aes_key_expansion(network_key, round_keys);
    aes_encrypt_block(round_keys, input, session_key);
}