#include <stdbool.h>
#include "../common/protocol.h"
/* Button is a manual report only; debounce and GATT transmission are HAL integration work. */
cu_event_t report_button(uint32_t seq, uint32_t ms, bool pressed) {
    return (cu_event_t){seq, ms, CU_MANUAL, pressed ? 1u : 0u};
}
