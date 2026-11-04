#include <stdint.h>
#include "../common/protocol.h"
/* SHT31 driver integration point; value is basis points of relative humidity. */
int main(void) {
    dc_event_t e = { DC_PROTOCOL_VERSION, 1u, DC_RH_BPS, 5000, 0u };
    return dc_event_valid(&e) ? 0 : 1;
}
