#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../common/protocol.h"

enum { NODE_HUB = 1u, NODE_MOBILITY = 4u };

static uint8_t radio_tx_buffer[160];

static size_t build_route_frame(uint16_t route_minutes,
                                int16_t eta_delta_minutes,
                                uint16_t pm25_x10,
                                uint16_t voc_index,
                                uint16_t vibration_rms_x100,
                                uint8_t crash_flag) {
    commute_frame_t frame;
    route_payload_t payload;
    commute_prepare_frame(&frame, MSG_ROUTE_SAMPLE, NODE_MOBILITY, NODE_HUB);

    payload.route_minutes = route_minutes;
    payload.eta_delta_minutes = eta_delta_minutes;
    payload.pm25_x10 = pm25_x10;
    payload.voc_index = voc_index;
    payload.vibration_rms_x100 = vibration_rms_x100;
    payload.crash_flag = crash_flag;

    frame.payload_length = sizeof(payload);
    memcpy(frame.payload, &payload, sizeof(payload));
    return commute_encode(&frame, radio_tx_buffer, sizeof(radio_tx_buffer));
}

static bool route_unhealthy(uint16_t pm25_x10, uint16_t vibration_rms_x100, int16_t eta_delta_minutes) {
    return pm25_x10 > 250u || vibration_rms_x100 > 120u || eta_delta_minutes > 10;
}

int main(void) {
    const size_t bytes = build_route_frame(33u, 7, 245u, 118u, 82u, 0u);
    volatile bool reroute = route_unhealthy(245u, 82u, 7);
    return (bytes > 0u && !reroute) ? 0 : 1;
}
