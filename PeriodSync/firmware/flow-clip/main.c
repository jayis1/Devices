#include <stdio.h>
#include "../common/mesh.h"
#include "../common/protocol.h"

static uint8_t classify_leak_risk(uint16_t cap_delta, uint8_t humidity, uint8_t posture) {
    uint16_t score = (cap_delta / 8U) + humidity;
    if (posture == 2) {
        score += 12;
    }
    if (score > 100) {
        score = 100;
    }
    return (uint8_t)score;
}

int main(void) {
    mesh_config_t cfg = { .node_id = 0x2002, .channel = 3, .tx_power_dbm = 10 };
    mesh_init(&cfg);

    uint16_t cap_delta = 248;
    uint8_t humidity = 67;
    uint8_t posture = 2;
    uint8_t leak_risk = classify_leak_risk(cap_delta, humidity, posture);

    uint8_t payload[5] = {
        (uint8_t)(cap_delta >> 8),
        (uint8_t)(cap_delta & 0xFF),
        humidity,
        posture,
        leak_risk
    };

    psync_frame_t frame;
    psync_build_frame(&frame, PSYNC_MSG_ALERT, cfg.node_id, 0x1001, 1724390400UL, 0x01, payload, sizeof(payload));
    mesh_send_frame(&frame);
    printf("flow_clip cap=%u humidity=%u posture=%u leak=%u\n", cap_delta, humidity, posture, leak_risk);
    return 0;
}
