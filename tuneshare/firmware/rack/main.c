#include "../common/hal.h"
#define TS_RACK_INPUT_COUNT 8u
void rack_publish(ts_hal_t *hal, const uint8_t *json, uint16_t len) {
    if (hal && hal->publish) hal->publish(2u, json, len);
}
