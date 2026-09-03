#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

typedef struct {
    float cabin_temp_c;
    float humidity_pct;
    float cry_score;
    bool thermal_child_shape;
} cabin_state_t;

static float heat_risk(const cabin_state_t *state) {
    float risk = (state->cabin_temp_c - 24.0f) * 3.0f + state->cry_score * 25.0f;
    if (state->thermal_child_shape) {
        risk += 20.0f;
    }
    if (risk < 0.0f) {
        risk = 0.0f;
    }
    return risk;
}

int main(void) {
    cabin_state_t state = { 34.5f, 48.0f, 0.32f, true };
    uint8_t payload[5] = {0};
    csync_frame_t frame;
    payload[0] = (uint8_t)(state.cabin_temp_c * 2.0f);
    payload[1] = (uint8_t)state.humidity_pct;
    payload[2] = (uint8_t)(state.cry_score * 100.0f);
    payload[3] = state.thermal_child_shape ? 1U : 0U;
    payload[4] = (uint8_t)heat_risk(&state);
    (void)csync_encode(&frame, 0x03U, MSG_CABIN_STATUS, 0xCA120001U, payload, 5U);
    printf("cabin risk=%u\n", payload[4]);
    return 0;
}
