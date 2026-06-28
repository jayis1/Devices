/*
 * PestSync Protocol implementation
 * psp_protocol.c
 */
#include "psp_protocol.h"
#include <string.h>

/* CRC-16 CCITT (0x1021, init 0xFFFF) */
static const uint16_t crc_table[256] = {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
    0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
    0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
    0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
    0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
    0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
    0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
    0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
    0x48C4,0x58E5,0x6886,0x7887,0x0840,0x1861,0x2802,0x3823,
    0xC9CC,0xD9ED,0xE98E,0xF9AF,0x8948,0x9969,0xA90A,0xB92B,
    0x5AF5,0x4AD4,0x7AB7,0x6A96,0x1A71,0x0A50,0x3A33,0x2A12,
    0xDBFD,0xCBDC,0xFBBF,0xEB9E,0x9B79,0x8B58,0xBB3B,0xAB1A,
    0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
    0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
    0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
    0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
    0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
    0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
    0x83B9,0x9398,0xA3FB,0xB3DA,0xE33D,0xF31C,0xC37F,0xD35E,
    0x2A90,0x3AB1,0x0AD2,0x1AF3,0x6B14,0x7B35,0x4B56,0x5B77,
    0xC6AC,0xD68D,0xE6EE,0xF6CF,0xA628,0xB609,0x866A,0x964B,
    0x061D,0x163C,0x265F,0x367E,0x4699,0x56B8,0x66DB,0x76FA,
    0x4DE0,0x5DC1,0x6DA2,0x7D83,0x07C4,0x17E5,0x2786,0x37A7,
    0x8448,0x9469,0xA40A,0xB42B,0x4FCC,0x5FED,0x6F8E,0x7FAF,
    0x1B90,0x0BB1,0x3BD2,0x2BF3,0x0074,0x1055,0x2076,0x3067,
    0xE8BC,0xF89D,0xC8FE,0xD8DF,0xA938,0xB919,0x997A,0x895B,
    0x58C7,0x48E6,0x7885,0x68A4,0x18C3,0x08E2,0x3881,0x28A0,
    0xDB6B,0xCB4A,0xFB29,0xEB08,0x9BEF,0x8BCE,0xBB8D,0xABAC,
    0x6C62,0x7C43,0x4C20,0x5C01,0x2C66,0x3C47,0x0C04,0x1C25,
    0x2DB4,0x3D95,0x0DF6,0x1DD7,0x6D30,0x7D11,0x4D52,0x5D73,
    0xF2E9,0xE2C8,0xD2AB,0xC28A,0xF34D,0xE36C,0xA30F,0xB32E,
    0x8259,0x9278,0xA21B,0xB23A,0xC2FD,0xD2DC,0xE2BF,0xF29E,
};

uint16_t psp_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return crc;
}

/* Simplified AES-128-CCM placeholder.
 * In production, use mbedTLS aes_ccm_encrypt / aes_ccm_decrypt.
 * This reference uses XOR stream + truncated CRC for structure demo. */

static void psp_encrypt(const uint8_t *key, const uint8_t *nonce, size_t nonce_len,
                         const uint8_t *plaintext, size_t pt_len,
                         uint8_t *ciphertext, uint8_t *mac, size_t mac_len)
{
    uint8_t stream = 0;
    for (size_t i = 0; i < nonce_len && i < AES_NONCE_SIZE; i++)
        stream ^= nonce[i];
    for (size_t i = 0; i < AES_KEY_SIZE; i++)
        stream ^= key[i];
    for (size_t i = 0; i < pt_len; i++) {
        ciphertext[i] = plaintext[i] ^ stream ^ (uint8_t)(i * 0x37 + 0xA5);
        stream = (stream << 1) | (stream >> 7);
    }
    uint16_t fake_mac = psp_crc16(ciphertext, pt_len);
    fake_mac ^= psp_crc16(key, AES_KEY_SIZE);
    mac[0] = (uint8_t)(fake_mac >> 8);
    mac[1] = (uint8_t)(fake_mac & 0xFF);
    mac[2] = nonce[0] ^ key[0];
    mac[3] = nonce[1] ^ key[1];
}

int psp_build_packet(uint8_t *buf, uint16_t src, uint16_t dst,
                     uint8_t msg_type, const uint8_t *payload,
                     uint8_t payload_len, uint8_t seq,
                     const uint8_t *aes_key)
{
    psp_header_t hdr;
    uint8_t encrypted[PSP_MAX_PAYLOAD + AES_MAC_SIZE];
    uint8_t nonce[AES_NONCE_SIZE];

    if (payload_len > PSP_MAX_PAYLOAD)
        return -1;

    hdr.msg_type    = msg_type;
    hdr.payload_len = payload_len;
    hdr.seq_num     = seq;
    hdr.src_id      = src;
    hdr.dst_id      = dst;
    hdr.timestamp   = 0;

    /* Build nonce: src(2) + seq(1) + pad(9) */
    nonce[0] = (uint8_t)(src >> 8);
    nonce[1] = (uint8_t)(src & 0xFF);
    nonce[2] = seq;
    memset(nonce + 3, 0x3C, 9);

    psp_encrypt(aes_key, nonce, AES_NONCE_SIZE,
                payload, payload_len, encrypted, encrypted + payload_len, AES_MAC_SIZE);

    size_t offset = 0;
    memcpy(buf + offset, &hdr, sizeof(hdr));
    offset += sizeof(hdr);
    memcpy(buf + offset, encrypted, payload_len);
    offset += payload_len;
    memcpy(buf + offset, encrypted + payload_len, AES_MAC_SIZE);
    offset += AES_MAC_SIZE;

    uint16_t crc = psp_crc16(buf, offset);
    buf[offset++] = (uint8_t)(crc >> 8);
    buf[offset++] = (uint8_t)(crc & 0xFF);

    return (int)offset;
}

int psp_parse_packet(const uint8_t *buf, size_t len,
                     psp_header_t *header, uint8_t *payload,
                     uint8_t *payload_len, const uint8_t *aes_key)
{
    if (len < sizeof(psp_header_t) + PSP_CRC_SIZE + AES_MAC_SIZE)
        return -1;

    memcpy(header, buf, sizeof(psp_header_t));

    if (header->payload_len > PSP_MAX_PAYLOAD)
        return -1;

    size_t total = sizeof(psp_header_t) + header->payload_len + AES_MAC_SIZE + PSP_CRC_SIZE;
    if (len < total)
        return -1;

    uint16_t crc_recv = (buf[total - 2] << 8) | buf[total - 1];
    uint16_t crc_calc = psp_crc16(buf, total - 2);
    if (crc_recv != crc_calc)
        return -2;

    /* Decrypt */
    uint8_t nonce[AES_NONCE_SIZE];
    nonce[0] = (uint8_t)(header->src_id >> 8);
    nonce[1] = (uint8_t)(header->src_id & 0xFF);
    nonce[2] = header->seq_num;
    memset(nonce + 3, 0x3C, 9);

    uint8_t stream = 0;
    for (size_t i = 0; i < AES_NONCE_SIZE; i++) stream ^= nonce[i];
    for (size_t i = 0; i < AES_KEY_SIZE; i++) stream ^= aes_key[i];
    for (size_t i = 0; i < header->payload_len; i++) {
        payload[i] = buf[sizeof(psp_header_t) + i] ^ stream ^ (uint8_t)(i * 0x37 + 0xA5);
        stream = (stream << 1) | (stream >> 7);
    }
    *payload_len = header->payload_len;

    return 0;
}

const char *pest_class_name(uint8_t pest_class)
{
    switch (pest_class) {
        case PEST_HOUSE_MOUSE:      return "House Mouse";
        case PEST_NORWAY_RAT:       return "Norway Rat";
        case PEST_GERMAN_ROACH:     return "German Cockroach";
        case PEST_AMERICAN_ROACH:   return "American Cockroach";
        case PEST_ARGENTINE_ANT:    return "Argentine Ant";
        case PEST_CARPENTER_ANT:    return "Carpenter Ant";
        case PEST_MOSQUITO:         return "Mosquito";
        case PEST_HOUSE_FLY:        return "House Fly";
        case PEST_FRUIT_FLY:        return "Fruit Fly";
        case PEST_BEDBUG:           return "Bedbug";
        case PEST_TERMITE_WORKER:   return "Termite (worker)";
        case PEST_TERMITE_SWARMER:  return "Termite (swarmer)";
        case PEST_SPIDER:           return "Spider";
        case PEST_SILVERFISH:       return "Silverfish";
        case PEST_CARPET_BEETLE:    return "Carpet Beetle";
        case PEST_NONE:             return "None";
        default:                    return "Unknown";
    }
}

const char *trap_status_name(uint8_t status)
{
    switch (status) {
        case TRAP_ARMED:        return "Armed";
        case TRAP_TRIGGERED:    return "Triggered";
        case TRAP_NEEDS_RESET:  return "Needs Reset";
        case TRAP_TAMPERED:     return "Tampered";
        default:                return "Unknown";
    }
}