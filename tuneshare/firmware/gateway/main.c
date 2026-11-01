#include "../common/protocol.h"
#include "../common/hal.h"
void gateway_on_message(ts_hal_t *hal, uint8_t kind, const uint8_t *data, uint16_t len) {
    if (hal && hal->publish && ts_payload_valid(data, len)) hal->publish(kind, data, len);
}
