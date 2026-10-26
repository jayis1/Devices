#include <stdint.h>
#include "../common/protocol.h"
/* Hardware-independent sampling/encoding core; board HAL and BLE transport are TODO. */
int cabinet_make_status(uint16_t node_id, uint32_t seq, uint32_t uptime,
                        int16_t temp_centi_c, uint16_t battery_mv,
                        uint16_t flags, uint8_t *frame, size_t capacity) {
    aedready_status_t s = {AEDREADY_VERSION, AEDREADY_KIND_STATUS, node_id, seq,
                           uptime, temp_centi_c, battery_mv, flags, 0};
    return (int)aedready_encode(&s, frame, capacity);
}
