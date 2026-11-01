#include "protocol.h"
int ts_payload_valid(const uint8_t *payload, size_t length) {
    if (payload == NULL || length == 0u || length > TS_MAX_PAYLOAD) return 0;
    /* JSON/schema parsing is performed by the gateway integration layer. */
    return payload[0] == '{' && payload[length - 1u] == '}';
}
