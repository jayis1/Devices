#include "protocol.h"
#include "crc16.h"
#include <string.h>

void cs_build_frame(cs_frame_t *frame,
                    uint16_t src,
                    uint16_t dst,
                    uint8_t msg_type,
                    uint16_t seq,
                    uint32_t session_nonce,
                    const uint8_t *payload,
                    uint8_t payload_len) {
    if (frame == NULL) {
        return;
    }
    memset(frame, 0, sizeof(*frame));
    frame->src_id = src;
    frame->dst_id = dst;
    frame->msg_type = msg_type;
    frame->seq = seq;
    frame->session_nonce = session_nonce;
    if (payload_len > CS_MAX_PAYLOAD) {
        payload_len = CS_MAX_PAYLOAD;
    }
    frame->payload_len = payload_len;
    if (payload != NULL && payload_len > 0u) {
        memcpy(frame->payload, payload, payload_len);
    }
    frame->crc = cs_crc16((const uint8_t *)frame, (uint16_t)(sizeof(*frame) - sizeof(frame->crc)));
}

bool cs_validate_frame(const cs_frame_t *frame) {
    if (frame == NULL || frame->payload_len > CS_MAX_PAYLOAD) {
        return false;
    }
    uint16_t crc = cs_crc16((const uint8_t *)frame, (uint16_t)(sizeof(*frame) - sizeof(frame->crc)));
    return crc == frame->crc;
}
