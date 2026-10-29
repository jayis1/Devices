#include "../common/protocol.h"
/* Bind DS18B20 and debounced rain-gauge GPIO drivers to the selected SDK. */
int weather_sample(float temperature_c, uint32_t tip_count) {
    (void)temperature_c; (void)tip_count;
    return 0; /* sampling integration intentionally unimplemented */
}
