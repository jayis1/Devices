#include <stdint.h>
#include <stdbool.h>

#define PIN_VOLTAGE_AIN 0u /* P0.02 / AIN0 */
#define PIN_CURRENT_AIN 1u /* P0.03 / AIN1 */
#define PIN_SERVICE 11u
#define PIN_STATUS_LED 13u
#define PIN_CASE 15u

struct report { uint8_t version; uint8_t node_id[8]; uint32_t seq; uint32_t uptime_s; uint16_t voltage_centivolt; uint16_t current_ma; int16_t temp_c_centi; uint16_t flags; };

/* Vendor SDK ADC, I2C, BLE, provisioning and persistent sequence drivers are TODO. */
static bool measurement_valid(uint16_t mv, uint16_t ma) { return mv <= 30000u && ma <= 30000u; }
int main(void) {
    struct report r = {0};
    r.version = 1u;
    r.flags = measurement_valid(r.voltage_centivolt, r.current_ma) ? 1u : 0u;
    (void)r;
    return 0;
}
