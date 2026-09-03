#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../common/protocol.h"

typedef struct {
    bool ignition_on;
    bool child_present;
    bool buckle_closed;
    bool caregiver_nearby;
    float cabin_temp_c;
    float heat_slope_c_per_min;
} hub_state_t;

static csync_alert_t evaluate_alert(const hub_state_t *state) {
    if (!state->buckle_closed && state->child_present) {
        return ALERT_HARNESS;
    }
    if (!state->ignition_on && state->child_present && state->cabin_temp_c > 39.0f && state->heat_slope_c_per_min > 0.35f) {
        return ALERT_HEAT;
    }
    if (!state->ignition_on && state->child_present && !state->caregiver_nearby) {
        return ALERT_MISSED_UNLOAD;
    }
    return ALERT_NONE;
}

int main(void) {
    hub_state_t state = { true, true, true, true, 27.5f, 0.02f };
    uint8_t payload[6] = {0};
    csync_frame_t frame;
    csync_alert_t alert = evaluate_alert(&state);
    payload[0] = (uint8_t)alert;
    payload[1] = (uint8_t)(state.cabin_temp_c * 2.0f);
    payload[2] = (uint8_t)(state.heat_slope_c_per_min * 100.0f);
    (void)csync_encode(&frame, 0x01U, MSG_ALERT, 0xCA120001U, payload, 3U);
    printf("vehicle-hub alert=%u crc=%u\n", (unsigned)alert, (unsigned)frame.crc16);
    return 0;
}
