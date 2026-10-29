#include "../common/protocol.h"
/* Bind read_surface_temp_c() and mqtt_publish() to ESP-IDF board drivers. */
int ramp_sample(float temperature_c, uint16_t adc_raw) {
    (void)temperature_c; (void)adc_raw;
    return 0; /* sampling integration intentionally unimplemented */
}
