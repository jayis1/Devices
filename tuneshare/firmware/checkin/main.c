#include "../common/hal.h"
void checkin_publish(ts_hal_t *hal, const uint8_t *json, uint16_t len) {
    if (hal && hal->publish) hal->publish(3u, json, len);
}
