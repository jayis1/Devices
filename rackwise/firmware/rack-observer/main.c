#include "../common/protocol.h"
#include <stdint.h>

/* GPIO/MQTT integration is platform-specific and intentionally not represented as complete. */
static rw_state_t stable_state(bool beam_blocked, bool channel_fault) {
    if (channel_fault) return RW_UNKNOWN;
    return beam_blocked ? RW_OCCUPIED : RW_CLEAR;
}
int main(void) {
    rw_rack_report_t report = {.seq = 1, .count = 2};
    uint8_t packet[14];
    report.bay[0] = stable_state(false, false);
    report.bay[1] = stable_state(false, true);
    return rw_encode(&report, packet, sizeof packet) == 8 ? 0 : 1;
}
