#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../common/protocol.h"

enum { NODE_HUB = 1u, NODE_ENTRY = 2u, NODE_BAG = 3u, NODE_MOBILITY = 4u, NODE_DESK = 5u };

static uint8_t downlink_buffer[160];

static size_t build_intervention_frame(uint8_t destination_node, uint8_t action_code, uint8_t severity) {
    commute_frame_t frame;
    commute_prepare_frame(&frame, MSG_INTERVENTION, NODE_HUB, destination_node);
    frame.payload[0] = action_code;
    frame.payload[1] = severity;
    frame.payload_length = 2u;
    return commute_encode(&frame, downlink_buffer, sizeof(downlink_buffer));
}

static uint8_t action_for_state(uint8_t missing_flags, int16_t eta_delta_minutes, uint8_t theft_score_x100) {
    if (theft_score_x100 >= 70u) {
        return 3u; /* high-priority security alert */
    }
    if (missing_flags != 0u) {
        return 1u; /* doorway reminder */
    }
    if (eta_delta_minutes > 8) {
        return 2u; /* reroute recommendation */
    }
    return 0u; /* no action */
}

int main(void) {
    const uint8_t action = action_for_state(0x02u, 7, 18u);
    const size_t bytes = build_intervention_frame(NODE_ENTRY, action, 1u);
    return (bytes > 0u && action == 1u) ? 0 : 1;
}
