/*
 * OralSync Sync Protocol (OSMP) — common header
 * Used by all nodes (Hub, Toothbrush, Plaque Scanner, Saliva Sensor)
 * over BLE 5.0 GATT and USB-CDC console.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef OSMP_H
#define OSMP_H

#include <stdint.h>
#include <stddef.h>

#define OSMP_SOP          0xAAu
#define OSMP_MAX_PAYLOAD  180u
#define OSMT_MAX_FRAME    (OSMP_MAX_PAYLOAD + 5u)
#define OSMP_SERVICE_UUID_LSB 0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, \
                              0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E

/* Message types — see docs/protocol.md */
enum osmp_type {
    OSMP_HELLO          = 0x01,
    OSMP_PAIR_REQ       = 0x02,
    OSMP_PAIR_ACK       = 0x03,
    OSMP_SESSION_START  = 0x10,
    OSMP_SESSION_END    = 0x11,
    OSMP_IMU_SAMPLE     = 0x12,
    OSMP_PRESSURE_SAMPLE= 0x13,
    OSMP_SCAN_FRAME     = 0x14,
    OSMP_SCAN_EMBED     = 0x15,
    OSMP_SALIVA_READING = 0x16,
    OSMP_COACH_CUE      = 0x20,
    OSMP_QUAD_PACE      = 0x21,
    OSMP_ACK            = 0x30,
    OSMP_NACK           = 0x31,
    OSMP_OTA_CHUNK      = 0x40,
    OSMP_OTA_DONE       = 0x41,
    OSMP_PING           = 0xF0,
    OSMP_PONG           = 0xF1,
};

enum osmp_node_type {
    OSMP_NODE_HUB       = 0x00,
    OSMP_NODE_TOOTHBRUSH= 0x01,
    OSMP_NODE_SCANNER   = 0x02,
    OSMP_NODE_SALIVA    = 0x03,
};

enum osmp_status {
    OSMP_OK             = 0x00,
    OSMP_ERR_CRC        = 0x01,
    OSMP_ERR_LEN        = 0x02,
    OSMP_ERR_TYPE       = 0x03,
    OSMP_ERR_AUTH       = 0x04,
    OSMP_ERR_BUSY       = 0x05,
};

#pragma pack(push, 1)
typedef struct {
    uint8_t  sop;
    uint8_t  len;
    uint8_t  type;
    uint8_t  seq;
    uint8_t  payload[OSMP_MAX_PAYLOAD];
    uint16_t crc;          /* little-endian over [len..payload] */
} osmp_frame_t;
#pragma pack(pop)

/* CRC-16/CCITT (poly 0x1021, init 0xFFFF, no refin/refout) */
uint16_t osmp_crc16(const uint8_t *data, size_t len);

/* Encode a frame into `out` (size >= len+5). Returns total frame size or 0 on error. */
size_t osmp_encode(uint8_t *out, size_t out_cap,
                   uint8_t type, uint8_t seq,
                   const uint8_t *payload, uint8_t payload_len);

/* Decode a frame from `in` of `in_len` bytes. Returns 1 on success, 0 on bad frame.
 * On success fills *type, *seq, *payload_len and copies payload into `payload_out`. */
int osmp_decode(const uint8_t *in, size_t in_len,
                uint8_t *type, uint8_t *seq,
                uint8_t *payload_out, uint8_t *payload_len);

/* Helper builders for common payloads. */
size_t osmp_build_hello(uint8_t *out, size_t cap, uint8_t node_type,
                        uint8_t hw_rev, uint16_t fw_ver, uint16_t caps);
size_t osmp_build_ack(uint8_t *out, size_t cap, uint8_t acked_seq, uint8_t status);
size_t osmp_build_imu(uint8_t *out, size_t cap, uint8_t seq,
                      int16_t ax, int16_t ay, int16_t az,
                      int16_t gx, int16_t gy, int16_t gz, uint16_t ts_ms);
size_t osmp_build_pressure(uint8_t *out, size_t cap, uint8_t seq,
                           uint16_t pressure_cN, uint16_t ts_ms);
size_t osmp_build_saliva(uint8_t *out, size_t cap, uint8_t seq,
                         uint16_t ph_x100, uint16_t nitrite_um,
                         uint8_t buffer, uint16_t temp_c10);
size_t osmp_build_session_end(uint8_t *out, size_t cap, uint8_t seq,
                              uint32_t session_id, uint16_t duration_s,
                              const uint8_t coverage[8]);

#endif /* OSMP_H */