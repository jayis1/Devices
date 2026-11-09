#include <stdint.h>
#include "../common/protocol.h"
/* Board-specific GPIO/ADC and secure radio drivers intentionally not included. */
int main(void) {
    volatile uint32_t door_closed = 0;
    volatile uint32_t load_band = 0;
    ss_event_t event = {SS_VERSION, 1u, (int32_t)(door_closed && load_band)};
    return event.version == SS_VERSION ? 0 : 1;
}
