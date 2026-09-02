#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "protocol.h"

typedef struct {
    float metric_a;
    float metric_b;
    float metric_c;
    bool alert;
} node_state_t;

static float compute_score(const node_state_t *state) {
    return sds_clampf((state->metric_a * 0.45f) + (state->metric_b * 0.35f) + (state->metric_c * 0.20f), 0.0f, 100.0f);
}

static sds_frame_t build_frame(uint16_t src, uint16_t dst, uint8_t room_id, float score, bool alert) {
    sds_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.src = src;
    frame.dst = dst;
    frame.room_id = room_id;
    frame.kind = alert ? SDS_KIND_ALERT : SDS_KIND_TELEMETRY;
    frame.epoch_s = 1700000000UL;
    frame.payload_len = 5U;
    frame.payload[0] = (uint8_t)score;
    frame.payload[1] = (uint8_t)(score * 10.0f) % 10U;
    frame.payload[2] = alert ? 1U : 0U;
    frame.payload[3] = (uint8_t)(src & 0xFFu);
    frame.payload[4] = room_id;
    return frame;
}

int main(void) {
    node_state_t state = { 28.0f, 32.0f, 26.0f, false };
    const float score = compute_score(&state);
    sds_frame_t frame = build_frame(4, 1U, 1, score, state.alert);
    uint8_t encoded[80];
    const size_t used = sds_encode_frame(&frame, encoded, sizeof(encoded));
    if (used == 0U) {
        return 1;
    }
    printf("med-station score=%.2f bytes=%zu kind=%u\n", score, used, frame.kind);
    return 0;
}
