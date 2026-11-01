#include "../common/protocol.h"
/* nRF5 SDK target; GPIO/advertising implementation intentionally pending. */
int main(void) {
    volatile ps_event_t event = { .version = PAWSTATION_PROTOCOL_VERSION, .seq = 0u, .kind = PS_SERVICE_CONFIRMED, .value = 1, .utc_ms = 0u };
    (void)event;
    return 0;
}
