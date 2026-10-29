#include "../common/protocol.h"

/* Target adapter: acquire calibrated soil probe and SHT40 values, then
 * call tt_encode() and pass the frame to the nRF Connect SDK BLE advertiser.
 * Sensor and radio drivers are deliberately platform-adapter TODOs. */
int main(void) {
    const tt_observation sample = {TT_VERSION, 1u, 0u, 1u, 0, 0u};
    uint8_t frame[TT_MAX_FRAME];
    return tt_encode(&sample, frame, sizeof frame) == 14u ? 0 : 1;
}
