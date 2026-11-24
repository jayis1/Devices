#include "protocol.h"
int hr_event_envelope_valid(const uint8_t *data, size_t length) {
    if (data == NULL || length == 0u || length > HR_MAX_EVENT_BYTES) return 0;
    if (data[0] != (uint8_t)'{' || data[length - 1u] != (uint8_t)'}') return 0;
    for (size_t i = 0; i < length; ++i) {
        if (data[i] == 0u || (data[i] < 0x20u && data[i] != '\n' && data[i] != '\r' && data[i] != '\t')) return 0;
    }
    return 1;
}
