/* ESP-IDF integration point; local-first event queue and MQTT TLS bridge. */
#include <stdint.h>
int gateway_event_allowed(uint32_t sequence, uint32_t previous) {
    return sequence > previous;
}
