#include <stdio.h>
#include <string.h>
#include "../common/protocol.h"
#include "../common/mesh.h"

static int classify_label_text(const char *text) {
    if (strstr(text, "peanut") || strstr(text, "tree nut")) {
        return 2;
    }
    if (strstr(text, "shared equipment") || strstr(text, "may contain")) {
        return 1;
    }
    return 0;
}

int main(void) {
    const char *ocr_text = "granola bar processed on shared equipment with peanuts";
    int risk = classify_label_text(ocr_text);

    fas_mesh_config_t config = {.node_id = 0x0011, .slot_index = 1, .uplink_period_ms = 5000, .retries = 2};
    fas_mesh_init(&config);

    fas_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.preamble = FAS_PREAMBLE;
    frame.version = FAS_VERSION;
    frame.message_type = risk ? FAS_MSG_ALERT : FAS_MSG_EVENT;
    frame.node_type = FAS_NODE_MEAL_SCANNER;
    frame.source_id = 0x0011;
    frame.destination_id = 0x0001;
    frame.payload_length = (uint8_t)snprintf((char *)frame.payload, FAS_MAX_PAYLOAD, "risk=%d", risk);
    fas_mesh_send(&frame);

    printf("[meal-scanner] OCR risk=%d\n", risk);
    return 0;
}
