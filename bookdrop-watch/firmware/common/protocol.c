#include "protocol.h"
int bd_payload_valid(const uint8_t *payload, size_t length) {
    if (payload == NULL || length == 0u || length > BD_MAX_PAYLOAD) return 0;
    return payload[0] == '{' && payload[length - 1u] == '}';
}
