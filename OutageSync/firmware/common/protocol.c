#include <string.h>
#include "protocol.h"
#include "crc16.h"

uint16_t os_crc16(const uint8_t *data, size_t len) {
    return os_crc16_ccitt(data, len);
}

void os_build_frame(os_frame_t *frame,
                    uint16_t src,
                    uint16_t dst,
                    uint8_t msg_type,
                    uint16_t seq,
                    uint32_t nonce,
                    const uint8_t *payload,
                    uint8_t payload_len) {
    memset(frame, 0, sizeof(*frame));
    frame->preamble[0] = 0xAA;
    frame->preamble[1] = 0x55;
    frame->preamble[2] = 0xAA;
    frame->preamble[3] = 0x55;
    frame->sync[0] = OS_SYNC_0;
    frame->sync[1] = OS_SYNC_1;
    frame->length = payload_len;
    frame->src_id = src;
    frame->dst_id = dst;
    frame->msg_type = msg_type;
    frame->seq = seq;
    frame->session_nonce = nonce;
    if (payload != NULL && payload_len > 0 && payload_len <= OS_PAYLOAD_MAX) {
        memcpy(frame->payload, payload, payload_len);
    }
    frame->crc = os_crc16((const uint8_t *)&frame->length,
                          (size_t)(1 + sizeof(frame->src_id) + sizeof(frame->dst_id) + sizeof(frame->msg_type) + sizeof(frame->seq) + sizeof(frame->session_nonce) + payload_len));
}

bool os_validate_frame(const os_frame_t *frame, uint8_t payload_len) {
    if (frame->sync[0] != OS_SYNC_0 || frame->sync[1] != OS_SYNC_1) return false;
    if (payload_len > OS_PAYLOAD_MAX || frame->length != payload_len) return false;
    uint16_t crc = os_crc16((const uint8_t *)&frame->length,
                            (size_t)(1 + sizeof(frame->src_id) + sizeof(frame->dst_id) + sizeof(frame->msg_type) + sizeof(frame->seq) + sizeof(frame->session_nonce) + payload_len));
    return crc == frame->crc;
}
