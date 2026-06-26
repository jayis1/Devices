/*
 * OSMP — protocol implementation (CRC + encode/decode + helpers).
 * MCU-independent C99, no libc beyond memset/memcpy.
 *
 * SPDX-License-Identifier: MIT
 */
#include "osmp.h"
#include <string.h>

uint16_t osmp_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000u) crc = (crc << 1) ^ 0x1021u;
            else               crc <<= 1;
        }
    }
    return crc;
}

size_t osmp_encode(uint8_t *out, size_t out_cap,
                   uint8_t type, uint8_t seq,
                   const uint8_t *payload, uint8_t payload_len)
{
    if (payload_len > OSMP_MAX_PAYLOAD) return 0;
    size_t need = (size_t)payload_len + 5u;
    if (out_cap < need) return 0;

    out[0] = OSMP_SOP;
    out[1] = payload_len;
    out[2] = type;
    out[3] = seq;
    if (payload_len) memcpy(&out[4], payload, payload_len);

    /* CRC over [len..payload] (i.e. bytes 1..3+payload_len) */
    uint16_t crc = osmp_crc16(&out[1], (size_t)payload_len + 3u);
    out[4u + payload_len]     = (uint8_t)(crc & 0xFFu);
    out[4u + payload_len + 1] = (uint8_t)(crc >> 8);
    return need;
}

int osmp_decode(const uint8_t *in, size_t in_len,
                uint8_t *type, uint8_t *seq,
                uint8_t *payload_out, uint8_t *payload_len)
{
    if (in_len < 5) return 0;
    if (in[0] != OSMP_SOP) return 0;
    uint8_t plen = in[1];
    if (in_len < (size_t)plen + 5u) return 0;

    uint16_t crc_expect = osmp_crc16(&in[1], (size_t)plen + 3u);
    uint16_t crc_got = (uint16_t)in[4u + plen] | ((uint16_t)in[4u + plen + 1] << 8);
    if (crc_expect != crc_got) return 0;

    *type = in[2];
    *seq  = in[3];
    *payload_len = plen;
    if (plen) memcpy(payload_out, &in[4], plen);
    return 1;
}

size_t osmp_build_hello(uint8_t *out, size_t cap, uint8_t node_type,
                        uint8_t hw_rev, uint16_t fw_ver, uint16_t caps)
{
    uint8_t p[6];
    p[0] = node_type; p[1] = hw_rev;
    p[2] = (uint8_t)(fw_ver & 0xFF); p[3] = (uint8_t)(fw_ver >> 8);
    p[4] = (uint8_t)(caps & 0xFF);   p[5] = (uint8_t)(caps >> 8);
    return osmp_encode(out, cap, OSMP_HELLO, 0, p, sizeof(p));
}

size_t osmp_build_ack(uint8_t *out, size_t cap, uint8_t acked_seq, uint8_t status)
{
    uint8_t p[2] = { acked_seq, status };
    return osmp_encode(out, cap, OSMP_ACK, 0, p, sizeof(p));
}

size_t osmp_build_imu(uint8_t *out, size_t cap, uint8_t seq,
                      int16_t ax, int16_t ay, int16_t az,
                      int16_t gx, int16_t gy, int16_t gz, uint16_t ts_ms)
{
    uint8_t p[14];
    memcpy(&p[0],  &ax, 2);
    memcpy(&p[2],  &ay, 2);
    memcpy(&p[4],  &az, 2);
    memcpy(&p[6],  &gx, 2);
    memcpy(&p[8],  &gy, 2);
    memcpy(&p[10], &gz, 2);
    memcpy(&p[12], &ts_ms, 2);
    return osmp_encode(out, cap, OSMP_IMU_SAMPLE, seq, p, sizeof(p));
}

size_t osmp_build_pressure(uint8_t *out, size_t cap, uint8_t seq,
                           uint16_t pressure_cN, uint16_t ts_ms)
{
    uint8_t p[4];
    memcpy(&p[0], &pressure_cN, 2);
    memcpy(&p[2], &ts_ms, 2);
    return osmp_encode(out, cap, OSMP_PRESSURE_SAMPLE, seq, p, sizeof(p));
}

size_t osmp_build_saliva(uint8_t *out, size_t cap, uint8_t seq,
                         uint16_t ph_x100, uint16_t nitrite_um,
                         uint8_t buffer, uint16_t temp_c10)
{
    uint8_t p[7];
    memcpy(&p[0], &ph_x100, 2);
    memcpy(&p[2], &nitrite_um, 2);
    p[4] = buffer;
    memcpy(&p[5], &temp_c10, 2);
    return osmp_encode(out, cap, OSMP_SALIVA_READING, seq, p, sizeof(p));
}

size_t osmp_build_session_end(uint8_t *out, size_t cap, uint8_t seq,
                              uint32_t session_id, uint16_t duration_s,
                              const uint8_t coverage[8])
{
    uint8_t p[14];
    memcpy(&p[0], &session_id, 4);
    memcpy(&p[4], &duration_s, 2);
    memcpy(&p[6], coverage, 8);
    return osmp_encode(out, cap, OSMP_SESSION_END, seq, p, sizeof(p));
}