#include <stdint.h>
#include "../common/protocol.h"
/* Sensor-driver integration point. Keep output advisory; no actuator is attached. */
int main(void) {
    dc_event_t e = { DC_PROTOCOL_VERSION, 1u, DC_MASS_G, 0, 0u };
    return dc_event_valid(&e) ? 0 : 1;
}
