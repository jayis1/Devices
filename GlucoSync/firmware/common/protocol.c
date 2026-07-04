/**
 * GlucoSync Shared Communication Protocol — Implementation
 *
 * License: MIT
 */

#include "protocol.h"

uint8_t glucosync_checksum(const uint8_t *data, uint8_t len)
{
    uint8_t cs = 0;
    for (uint8_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}

uint8_t glucosync_encode(uint8_t *buf, uint8_t buf_len,
                         glucosync_msg_type_t msg_type,
                         uint16_t sender_id, uint16_t seq_num,
                         uint8_t flags,
                         const uint8_t *payload, uint8_t payload_len)
{
    if (buf == NULL || payload_len > GS_MAX_PAYLOAD) {
        return 0;
    }

    uint8_t total = GS_HEADER_LEN + payload_len;
    if (buf_len < total) {
        return 0;
    }

    buf[0]  = GS_SYNC_BYTE_0;
    buf[1]  = GS_SYNC_BYTE_1;
    buf[2]  = GS_PROTOCOL_VERSION;
    buf[3]  = (uint8_t)msg_type;
    buf[4]  = (uint8_t)(sender_id & 0xFF);
    buf[5]  = (uint8_t)(sender_id >> 8);
    buf[6]  = (uint8_t)(seq_num & 0xFF);
    buf[7]  = (uint8_t)(seq_num >> 8);
    buf[8]  = flags;
    buf[9]  = payload_len;

    buf[10] = glucosync_checksum(buf, 10);

    if (payload != NULL && payload_len > 0) {
        memcpy(&buf[11], payload, payload_len);
    }

    return total;
}

bool glucosync_decode(const uint8_t *buf, uint8_t buf_len,
                      glucosync_header_t *header,
                      const uint8_t **payload)
{
    if (buf == NULL || buf_len < GS_HEADER_LEN) {
        return false;
    }

    if (!glucosync_validate(buf, buf_len)) {
        return false;
    }

    if (header != NULL) {
        header->sync[0]     = buf[0];
        header->sync[1]     = buf[1];
        header->version     = buf[2];
        header->msg_type    = buf[3];
        header->sender_id   = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
        header->seq_num     = (uint16_t)buf[6] | ((uint16_t)buf[7] << 8);
        header->flags       = buf[8];
        header->payload_len = buf[9];
        header->checksum    = buf[10];
    }

    if (payload != NULL) {
        *payload = &buf[11];
    }

    return true;
}

bool glucosync_validate(const uint8_t *buf, uint8_t buf_len)
{
    if (buf == NULL || buf_len < GS_HEADER_LEN) {
        return false;
    }

    if (buf[0] != GS_SYNC_BYTE_0 || buf[1] != GS_SYNC_BYTE_1) {
        return false;
    }

    if (buf[2] != GS_PROTOCOL_VERSION) {
        return false;
    }

    uint8_t payload_len = buf[9];
    if (buf_len < (uint8_t)(GS_HEADER_LEN + payload_len)) {
        return false;
    }

    uint8_t computed = glucosync_checksum(buf, 10);
    if (computed != buf[10]) {
        return false;
    }

    return true;
}