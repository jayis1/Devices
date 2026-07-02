/**
 * JointSync Shared Communication Protocol — Implementation
 *
 * License: MIT
 */

#include "protocol.h"

uint8_t jointsync_checksum(const uint8_t *data, uint8_t len)
{
    uint8_t cs = 0;
    for (uint8_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}

uint8_t jointsync_encode(uint8_t *buf, uint8_t buf_len,
                         jointsync_msg_type_t msg_type,
                         uint16_t sender_id, uint16_t seq_num,
                         uint8_t flags,
                         const uint8_t *payload, uint8_t payload_len)
{
    if (buf == NULL || payload_len > JS_MAX_PAYLOAD) {
        return 0;
    }

    uint8_t total = JS_HEADER_LEN + payload_len;
    if (buf_len < total) {
        return 0;
    }

    /* Build header in buffer */
    buf[0]  = JS_SYNC_BYTE_0;
    buf[1]  = JS_SYNC_BYTE_1;
    buf[2]  = JS_PROTOCOL_VERSION;
    buf[3]  = (uint8_t)msg_type;
    buf[4]  = (uint8_t)(sender_id & 0xFF);
    buf[5]  = (uint8_t)(sender_id >> 8);
    buf[6]  = (uint8_t)(seq_num & 0xFF);
    buf[7]  = (uint8_t)(seq_num >> 8);
    buf[8]  = flags;
    buf[9]  = payload_len;

    /* Checksum over bytes 0..9 (10 bytes, skip checksum field at [10]) */
    buf[10] = jointsync_checksum(buf, 10);

    /* Copy payload */
    if (payload != NULL && payload_len > 0) {
        memcpy(&buf[11], payload, payload_len);
    }

    return total;
}

bool jointsync_decode(const uint8_t *buf, uint8_t buf_len,
                      jointsync_header_t *header,
                      const uint8_t **payload)
{
    if (buf == NULL || buf_len < JS_HEADER_LEN) {
        return false;
    }

    if (!jointsync_validate(buf, buf_len)) {
        return false;
    }

    if (header != NULL) {
        header->sync[0]     = buf[0];
        header->sync[1]     = buf[1];
        header->version     = buf[2];
        header->msg_type     = buf[3];
        header->sender_id    = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
        header->seq_num      = (uint16_t)buf[6] | ((uint16_t)buf[7] << 8);
        header->flags        = buf[8];
        header->payload_len  = buf[9];
        header->checksum     = buf[10];
    }

    if (payload != NULL) {
        *payload = &buf[11];
    }

    return true;
}

bool jointsync_validate(const uint8_t *buf, uint8_t buf_len)
{
    if (buf == NULL || buf_len < JS_HEADER_LEN) {
        return false;
    }

    /* Check sync bytes */
    if (buf[0] != JS_SYNC_BYTE_0 || buf[1] != JS_SYNC_BYTE_1) {
        return false;
    }

    /* Check version */
    if (buf[2] != JS_PROTOCOL_VERSION) {
        return false;
    }

    /* Check payload length fits in buffer */
    uint8_t payload_len = buf[9];
    if (buf_len < (uint8_t)(JS_HEADER_LEN + payload_len)) {
        return false;
    }

    /* Verify checksum (over bytes 0..9, compare with byte 10) */
    uint8_t computed = jointsync_checksum(buf, 10);
    if (computed != buf[10]) {
        return false;
    }

    return true;
}