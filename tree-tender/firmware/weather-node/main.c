#include "../common/protocol.h"

/* Target adapter: sample SHT40/BMP390 and rain reed counter, then advertise
 * via nRF Connect SDK BLE. This portable skeleton does not access hardware. */
int main(void) {
    const tt_observation sample = {TT_VERSION, 2u, 0u, 2u, 0, 0u};
    uint8_t frame[TT_MAX_FRAME];
    return tt_encode(&sample, frame, sizeof frame) == 14u ? 0 : 1;
}
