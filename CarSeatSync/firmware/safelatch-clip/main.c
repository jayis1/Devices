#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

typedef struct {
    bool buckle_closed;
    float strap_tension_n;
    float chest_clip_height_ratio;
    bool strap_twisted;
} safelatch_state_t;

static uint8_t misuse_score(const safelatch_state_t *state) {
    uint8_t score = 0U;
    if (!state->buckle_closed) score += 50U;
    if (state->strap_tension_n < 24.0f) score += 25U;
    if (state->chest_clip_height_ratio < 0.45f || state->chest_clip_height_ratio > 0.70f) score += 15U;
    if (state->strap_twisted) score += 10U;
    return score;
}

int main(void) {
    safelatch_state_t state = { true, 31.2f, 0.58f, false };
    uint8_t payload[4] = {0};
    csync_frame_t frame;
    payload[0] = state.buckle_closed ? 1U : 0U;
    payload[1] = (uint8_t)state.strap_tension_n;
    payload[2] = (uint8_t)(state.chest_clip_height_ratio * 100.0f);
    payload[3] = misuse_score(&state);
    (void)csync_encode(&frame, 0x02U, MSG_SEAT_STATUS, 0xCA120001U, payload, 4U);
    printf("safelatch misuse=%u valid=%d\n", payload[3], csync_validate(&frame));
    return 0;
}
