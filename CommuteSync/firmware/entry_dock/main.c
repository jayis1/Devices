#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../common/protocol.h"

enum { NODE_HUB = 1u, NODE_ENTRY = 2u };

static uint8_t radio_tx_buffer[160];

static uint8_t compute_missing_flags(bool bag_present, bool badge_seen, bool keys_seen) {
    uint8_t flags = 0u;
    if (!bag_present) {
        flags |= 0x01u;
    }
    if (!badge_seen) {
        flags |= 0x02u;
    }
    if (!keys_seen) {
        flags |= 0x04u;
    }
    return flags;
}

static size_t build_readiness_frame(uint8_t required_items,
                                    uint8_t confirmed_items,
                                    bool bag_present,
                                    bool badge_seen,
                                    bool keys_seen,
                                    uint8_t departure_in_minutes) {
    commute_frame_t frame;
    readiness_payload_t payload;
    commute_prepare_frame(&frame, MSG_READINESS, NODE_ENTRY, NODE_HUB);

    payload.required_items = required_items;
    payload.confirmed_items = confirmed_items;
    payload.missing_flags = compute_missing_flags(bag_present, badge_seen, keys_seen);
    payload.departure_in_minutes = departure_in_minutes;

    frame.payload_length = sizeof(payload);
    memcpy(frame.payload, &payload, sizeof(payload));
    return commute_encode(&frame, radio_tx_buffer, sizeof(radio_tx_buffer));
}

static bool readiness_blocking(uint8_t required_items, uint8_t confirmed_items, uint8_t departure_in_minutes) {
    if (confirmed_items < required_items) {
        return true;
    }
    return departure_in_minutes <= 5u;
}

int main(void) {
    const size_t bytes = build_readiness_frame(5u, 4u, true, false, true, 12u);
    volatile bool should_prompt = readiness_blocking(5u, 4u, 12u);
    return (bytes > 0u && should_prompt) ? 0 : 1;
}
