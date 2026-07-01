/**
 * MigraineSync — Common Protocol Implementation
 * =============================================
 * TLV frame encode/decode + CRC-16/CCITT.
 *
 * License: MIT
 */

#include "protocol.h"
#include <string.h>

/* ── CRC-16/CCITT (0x1021, init 0xFFFF) ─────────────────── */
static const uint16_t crc_table[256] = {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
    0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
    0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
    0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
    0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
    0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
    0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
    0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
    0x48C4,0x58E5,0x6886,0x78A7,0x0840,0x1861,0x2802,0x3823,
    0xC9CC,0xD9ED,0xE98E,0xF9AF,0x8948,0x9969,0xA90A,0xB92B,
    0x5AF5,0x4AD4,0x7AB7,0x6A96,0x1A71,0x0A50,0x3A33,0x2A12,
    0xDBFD,0xCBDC,0xFBBF,0xEB9E,0x9B79,0x8B58,0xBB3B,0xAB1A,
    0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
    0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
    0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
    0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
    0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
    0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x6046,0x7067,
    0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,
    0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
    0xB5EA,0xA5CB,0x95A8,0x8589,0xF56E,0xE54F,0xD52C,0xC50D,
    0x34E2,0x24C3,0x14A0,0x0481,0x7466,0x6447,0x54A4,0x4485,
    0x86D3,0x96F2,0xA691,0xB6B0,0xC657,0xD676,0xE615,0xF634,
    0x079B,0x17BA,0x27D9,0x37F8,0x471F,0x573E,0x675D,0x777C,
    0xA99C,0xB9BD,0x89DE,0x99FF,0xE918,0xF939,0xC95A,0xD97B,
    0x3A86,0x2AA7,0x1AC4,0x0AE5,0x7A02,0x6A23,0x5A40,0x4A61,
    0xFBC4,0xEBE5,0xDB86,0xCBA7,0xBB40,0xAB61,0x9B22,0x8B43,
    0x4347,0x5366,0x6305,0x7324,0x03C3,0x13E2,0x27A1,0x3780,
    0x599B,0x69BA,0x79D9,0x49F8,0x591B,0x693A,0x7959,0x4978,
    0x8847,0x9866,0xA805,0xB824,0x48C3,0x58E2,0x2881,0x38A0,
    0xCB41,0xDB60,0xEB03,0xFB22,0x8BC5,0x9BE4,0xAB87,0xBBA6,
    0x4C59,0x5C78,0x6C1B,0x7C3A,0x4CDD,0x5CFC,0x6C9F,0x7CBE,
};

uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
        crc = (crc << 8) ^ crc_table[((crc >> 8) ^ data[i]) & 0xFF];
    return crc;
}

/* ── Encoding helpers ───────────────────────────────────── */
void encode_f32(uint8_t *buf, float val)
{
    uint32_t u;
    memcpy(&u, &val, 4);
    buf[0] = u & 0xFF;
    buf[1] = (u >> 8) & 0xFF;
    buf[2] = (u >> 16) & 0xFF;
    buf[3] = (u >> 24) & 0xFF;
}

float decode_f32(const uint8_t *buf)
{
    uint32_t u = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                 ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

void encode_u16(uint8_t *buf, uint16_t val)
{
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
}

uint16_t decode_u16(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

/* ── Frame build ────────────────────────────────────────── */
size_t frame_build(frame_t *frame, uint8_t seq,
                    const tlv_t *tlvs, int n_tlvs)
{
    size_t offset = 0;

    for (int i = 0; i < n_tlvs && offset < MAX_PAYLOAD - 2; i++) {
        if (offset + 2 + tlvs[i].len > MAX_PAYLOAD)
            break;

        frame->payload[offset++] = tlvs[i].type;
        frame->payload[offset++] = tlvs[i].len;
        memcpy(&frame->payload[offset], tlvs[i].value, tlvs[i].len);
        offset += tlvs[i].len;
    }

    frame->sop = SOP_BYTE;
    frame->seq = seq;
    frame->len = (uint8_t)offset;

    /* CRC over seq + len + payload */
    uint8_t crc_buf[2 + MAX_PAYLOAD];
    crc_buf[0] = frame->seq;
    crc_buf[1] = frame->len;
    memcpy(&crc_buf[2], frame->payload, offset);
    frame->crc = crc16_ccitt(crc_buf, 2 + offset);

    return 4 + offset + 2;  /* sop + seq + len + payload + crc */
}

/* ── Frame parse ────────────────────────────────────────── */
int frame_parse(const uint8_t *buf, size_t buf_len,
                frame_t *out_frame, tlv_t *out_tlvs, int *out_n_tlvs)
{
    if (buf_len < 6)  /* minimum: sop + seq + len(0) + crc */
        return -2;
    if (buf[0] != SOP_BYTE)
        return -2;

    size_t payload_len = buf[2];
    if (buf_len < 4 + payload_len + 2)
        return -2;

    /* Verify CRC */
    uint8_t crc_buf[2 + MAX_PAYLOAD];
    crc_buf[0] = buf[1];  /* seq */
    crc_buf[1] = buf[2];  /* len */
    memcpy(&crc_buf[2], &buf[3], payload_len);
    uint16_t expected = crc16_ccitt(crc_buf, 2 + payload_len);
    uint16_t received = (uint16_t)buf[3 + payload_len] |
                        ((uint16_t)buf[3 + payload_len + 1] << 8);
    if (expected != received)
        return -1;

    /* Copy into frame struct */
    out_frame->sop = buf[0];
    out_frame->seq = buf[1];
    out_frame->len = (uint8_t)payload_len;
    memcpy(out_frame->payload, &buf[3], payload_len);
    out_frame->crc = received;

    /* Parse TLVs */
    int n = 0;
    size_t offset = 0;
    while (offset < payload_len && n < MAX_TLVS) {
        uint8_t t = out_frame->payload[offset++];
        uint8_t l = out_frame->payload[offset++];
        if (offset + l > payload_len)
            return -2;

        out_tlvs[n].type = t;
        out_tlvs[n].len = l;
        out_tlvs[n].value = &out_frame->payload[offset];
        offset += l;
        n++;
    }

    *out_n_tlvs = n;
    return 0;
}