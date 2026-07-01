/**
 * MigraineSync — Common Protocol Header
 * =====================================
 * Shared TLV message protocol for all MigraineSync nodes.
 * Used over Sub-GHz (SX1262) and BLE (GATT notify).
 *
 * License: MIT
 */

#ifndef MIGRAINESYNC_PROTOCOL_H
#define MIGRAINESYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ── Frame constants ────────────────────────────────────── */
#define SOP_BYTE        0xA5
#define MAX_PAYLOAD     128
#define MAX_TLVS        8

/* ── Message types ──────────────────────────────────────── */
enum msg_type {
    MSG_ENVIRONMENT   = 0x01,
    MSG_VITALS        = 0x02,
    MSG_BAROMETRIC    = 0x03,
    MSG_LIGHT_DOSE    = 0x04,
    MSG_HYDRATION     = 0x05,
    MSG_ALERT         = 0x06,
    MSG_MANUAL_EVENT  = 0x07,
    MSG_BATTERY       = 0x08,
    MSG_TIME_SYNC     = 0x09,
    MSG_PAIR_REQ      = 0x0A,
    MSG_PAIR_ACK      = 0x0B,
    MSG_FW_OTA        = 0x0C,
};

/* ── Node IDs ───────────────────────────────────────────── */
enum node_id {
    NODE_HUB          = 0x0001,
    NODE_ENV_SENTINEL = 0x0002,
    NODE_AURA_BAND    = 0x0003,
    NODE_HYDRATE_TAG  = 0x0004,
};

/* ── Frame structure ────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  sop;
    uint8_t  seq;
    uint8_t  len;       /* payload length */
    uint8_t  payload[MAX_PAYLOAD];
    uint16_t crc;       /* CRC-16/CCITT over seq + len + payload */
} frame_t;

/* ── TLV field ──────────────────────────────────────────── */
typedef struct {
    uint8_t  type;
    uint8_t  len;
    const uint8_t *value;
} tlv_t;

/* ── Functions ──────────────────────────────────────────── */

/**
 * Build a frame from an array of TLV fields.
 * Returns total frame size, or 0 on error.
 */
size_t frame_build(frame_t *frame, uint8_t seq,
                    const tlv_t *tlvs, int n_tlvs);

/**
 * Parse a received byte buffer into a frame and extract TLV fields.
 * Returns 0 on success, -1 on CRC error, -2 on format error.
 */
int frame_parse(const uint8_t *buf, size_t buf_len,
                frame_t *out_frame, tlv_t *out_tlvs, int *out_n_tlvs);

/**
 * Compute CRC-16/CCITT.
 */
uint16_t crc16_ccitt(const uint8_t *data, size_t len);

/**
 * Encode a float into 4 bytes (little-endian).
 */
void encode_f32(uint8_t *buf, float val);

/**
 * Decode a float from 4 bytes (little-endian).
 */
float decode_f32(const uint8_t *buf);

/**
 * Encode a uint16 into 2 bytes (little-endian).
 */
void encode_u16(uint8_t *buf, uint16_t val);

/**
 * Decode a uint16 from 2 bytes (little-endian).
 */
uint16_t decode_u16(const uint8_t *buf);

#endif /* MIGRAINESYNC_PROTOCOL_H */