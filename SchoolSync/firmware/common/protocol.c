#include "protocol.h"
#include "crc16.h"
#include <string.h>

uint16_t ss_crc16(const uint8_t *data, size_t len) {
    return crc16_ccitt(data, len);
}

void ss_build_frame(ss_frame_t *frame,
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
    frame->sync[0] = SS_SYNC_0;
    frame->sync[1] = SS_SYNC_1;
    frame->length = payload_len;
    frame->src_id = src;
    frame->dst_id = dst;
    frame->msg_type = msg_type;
    frame->seq = seq;
    frame->session_nonce = nonce;
    if (payload != NULL && payload_len > 0) {
        memcpy(frame->payload, payload, payload_len);
    }
    frame->crc = ss_crc16((const uint8_t *)frame, offsetof(ss_frame_t, crc));
}

bool ss_validate_frame(const ss_frame_t *frame, uint8_t payload_len) {
    if (frame->sync[0] != SS_SYNC_0 || frame->sync[1] != SS_SYNC_1) {
        return false;
    }
    if (frame->length != payload_len) {
        return false;
    }
    uint16_t calc = ss_crc16((const uint8_t *)frame, offsetof(ss_frame_t, crc));
    return calc == frame->crc;
}
