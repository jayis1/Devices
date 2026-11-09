#include <stdint.h>
#include "../common/protocol.h"
/* Board-specific one-wire and pulse-counter drivers intentionally not included. */
int main(void) {
    volatile uint32_t pulses = 0;
    ss_event_t event = {SS_VERSION, 1u, (int32_t)pulses};
    return event.version == SS_VERSION ? 0 : 1;
}
