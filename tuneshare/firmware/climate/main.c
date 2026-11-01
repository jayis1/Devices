#include "../common/hal.h"
#define TS_SENSOR_GPIO_SDA 6
#define TS_SENSOR_GPIO_SCL 7
void climate_sample(ts_hal_t *hal, const uint8_t *json, uint16_t len) {
    if (hal && hal->publish) hal->publish(1u, json, len);
}
