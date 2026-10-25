#include <stdbool.h>
#include "../common/protocol.h"
/* HAL-independent sensor policy; integrate with Zephyr GPIO/I2C/ADC and BLE GATT. */
static bool wet, tilted;
cu_event_t curb_sample(uint32_t seq, uint32_t ms, bool wet_in, bool tilt_in) {
    wet = wet_in; tilted = tilt_in;
    return (cu_event_t){seq, ms, wet ? CU_WET : (tilted ? CU_TILT : CU_OBSTRUCTION), (uint8_t)(wet || tilted)};
}
