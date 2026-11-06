#include <stdint.h>
/* Window sentinel application hook: platform reads VEML7700/SHT31 and sends
 * a versioned, MAC-authenticated room report through BLE GATT. */
int window_report_valid(float lux, float temp_c, float rh) {
    return lux >= 0 && lux <= 120000 && temp_c >= -40 && temp_c <= 85 && rh >= 0 && rh <= 100;
}
