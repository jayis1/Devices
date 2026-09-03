#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

typedef struct {
    bool child_seen;
    bool bag_seen;
    bool medicine_tapped;
    uint16_t distance_cm;
} handoff_state_t;

static uint8_t completion_code(const handoff_state_t *state) {
    if (state->child_seen && state->bag_seen && state->medicine_tapped) {
        return 3U;
    }
    if (state->child_seen && state->bag_seen) {
        return 2U;
    }
    if (state->child_seen) {
        return 1U;
    }
    return 0U;
}

int main(void) {
    handoff_state_t state = { true, true, false, 95U };
    uint8_t payload[5] = {0};
    csync_frame_t frame;
    payload[0] = state.child_seen ? 1U : 0U;
    payload[1] = state.bag_seen ? 1U : 0U;
    payload[2] = state.medicine_tapped ? 1U : 0U;
    payload[3] = (uint8_t)(state.distance_cm & 0xFFU);
    payload[4] = completion_code(&state);
    (void)csync_encode(&frame, 0x05U, MSG_HANDOFF_EVENT, 0xCA120001U, payload, 5U);
    printf("handoff completion=%u\n", payload[4]);
    return 0;
}
