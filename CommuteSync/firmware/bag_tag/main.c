#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../common/protocol.h"

enum { NODE_HUB = 1u, NODE_BAG = 3u };

static uint8_t radio_tx_buffer[160];

static size_t build_bag_frame(uint8_t tamper_score_x100,
                              uint8_t motion_state,
                              uint16_t separation_cm,
                              uint16_t battery_mv) {
    commute_frame_t frame;
    bag_payload_t payload;
    commute_prepare_frame(&frame, MSG_BAG_TAMPER, NODE_BAG, NODE_HUB);

    payload.tamper_score_x100 = tamper_score_x100;
    payload.motion_state = motion_state;
    payload.separation_cm = separation_cm;
    payload.battery_mv = battery_mv;

    frame.payload_length = sizeof(payload);
    memcpy(frame.payload, &payload, sizeof(payload));
    return commute_encode(&frame, radio_tx_buffer, sizeof(radio_tx_buffer));
}

static bool tamper_likely(uint8_t tamper_score_x100, uint16_t separation_cm) {
    return tamper_score_x100 >= 60u || separation_cm > 250u;
}

int main(void) {
    const size_t bytes = build_bag_frame(18u, 0u, 60u, 2920u);
    volatile bool alert = tamper_likely(18u, 60u);
    return (bytes > 0u && !alert) ? 0 : 1;
}
