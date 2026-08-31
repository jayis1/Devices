#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../common/protocol.h"

typedef struct {
    uint8_t band_online;
    uint8_t cuff_online;
    uint8_t strip_online;
    uint8_t pad_online;
    uint8_t risk_level;
} hub_state_t;

static void publish_summary(const hub_state_t *state) {
    printf("hub summary: band=%u cuff=%u strip=%u pad=%u risk=%u\n",
           state->band_online, state->cuff_online, state->strip_online, state->pad_online, state->risk_level);
}

static bool build_action_frame(psync_frame_t *frame, uint8_t risk_level) {
    const char *message = (risk_level >= 75U) ? "ESCALATE" : "COACH";
    size_t len = strlen(message);
    memset(frame, 0, sizeof(*frame));
    frame->version = 1U;
    frame->type = PSYNC_MSG_ACTION;
    frame->source = 1U;
    frame->destination = 0xFFU;
    frame->payload_length = (uint16_t)len;
    memcpy(frame->payload, message, len);
    return true;
}

int main(void) {
    hub_state_t state = {1U, 1U, 1U, 1U, 58U};
    psync_frame_t frame;
    uint8_t raw[128];
    size_t written = 0U;

    publish_summary(&state);
    if (build_action_frame(&frame, state.risk_level) && psync_encode(&frame, raw, sizeof raw, &written)) {
        printf("encoded action frame bytes=%zu crc=0x%04X\n", written, frame.crc16);
    }
    return 0;
}
