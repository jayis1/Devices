#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

typedef struct {
    float skin_temp_c;
    uint8_t heart_rate_bpm;
    bool asleep;
    bool chin_to_chest;
} band_state_t;

static uint8_t posture_risk(const band_state_t *state) {
    uint8_t risk = 0U;
    if (state->asleep) risk += 25U;
    if (state->chin_to_chest) risk += 45U;
    if (state->skin_temp_c > 37.8f) risk += 20U;
    return risk;
}

int main(void) {
    band_state_t state = { 36.7f, 108U, true, false };
    uint8_t payload[4] = {0};
    csync_frame_t frame;
    payload[0] = (uint8_t)(state.skin_temp_c * 10.0f);
    payload[1] = state.heart_rate_bpm;
    payload[2] = state.asleep ? 1U : 0U;
    payload[3] = posture_risk(&state);
    (void)csync_encode(&frame, 0x04U, MSG_CHILD_STATUS, 0xCA120001U, payload, 4U);
    printf("child-band posture-risk=%u\n", payload[3]);
    return 0;
}
