#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../common/protocol.h"

enum { NODE_HUB = 1u, NODE_DESK = 5u };

static uint8_t radio_tx_buffer[160];

static size_t build_arrival_frame(uint8_t arrival_confirmed,
                                  uint8_t items_left_behind,
                                  uint8_t laptop_present,
                                  uint8_t bag_present) {
    commute_frame_t frame;
    arrival_payload_t payload;
    commute_prepare_frame(&frame, MSG_ARRIVAL, NODE_DESK, NODE_HUB);

    payload.arrival_confirmed = arrival_confirmed;
    payload.items_left_behind = items_left_behind;
    payload.laptop_present = laptop_present;
    payload.bag_present = bag_present;

    frame.payload_length = sizeof(payload);
    memcpy(frame.payload, &payload, sizeof(payload));
    return commute_encode(&frame, radio_tx_buffer, sizeof(radio_tx_buffer));
}

static bool leaving_warning(uint8_t items_left_behind) {
    return items_left_behind > 0u;
}

int main(void) {
    const size_t bytes = build_arrival_frame(1u, 0u, 1u, 1u);
    volatile bool warn = leaving_warning(0u);
    return (bytes > 0u && !warn) ? 0 : 1;
}
