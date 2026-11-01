#include "../common/protocol.h"
/* ESP-IDF application target: sensor/radio driver integration intentionally pending. */
int app_main(void) {
    volatile ps_event_t event = { .version = PAWSTATION_PROTOCOL_VERSION, .seq = 0u, .kind = PS_BIN_FILL, .value = 0, .utc_ms = 0u };
    (void)event;
    return 0;
}
