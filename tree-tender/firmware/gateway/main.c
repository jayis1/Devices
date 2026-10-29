#include "../common/protocol.h"

/* ESP-IDF integration point: scan BLE advertisements, validate/decode frames,
 * queue observations locally, and publish to MQTT over TLS. Driver/network
 * integration is a prototype TODO, not an implemented gateway. */
int main(void) {
    tt_observation decoded;
    const uint8_t example[14] = {1,1,0,0,0,1,0,0,0,0,0,0,0,0};
    (void)tt_decode(example, sizeof example, &decoded);
    return 0;
}
