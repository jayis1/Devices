#include "protocol.h"
#include <string.h>

uint16_t ws_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

void ws_build_frame(ws_frame_t *frame,
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
    frame->sync[0] = WS_SYNC_0;
    frame->sync[1] = WS_SYNC_1;
    frame->length = (uint8_t)(11u + payload_len);
    frame->src_id = src;
    frame->dst_id = dst;
    frame->msg_type = msg_type;
    frame->seq = seq;
    frame->session_nonce = nonce;
    if (payload && payload_len > 0 && payload_len <= WS_FRAME_PAYLOAD_MAX) {
        memcpy(frame->payload, payload, payload_len);
    }
    frame->crc = ws_crc16((const uint8_t *)&frame->length, (size_t)(11u + payload_len));
}

bool ws_validate_frame(const ws_frame_t *frame, uint8_t payload_len) {
    if (frame->sync[0] != WS_SYNC_0 || frame->sync[1] != WS_SYNC_1) {
        return false;
    }
    uint16_t expected = ws_crc16((const uint8_t *)&frame->length, (size_t)(11u + payload_len));
    return frame->crc == expected;
}
