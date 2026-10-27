#include <stdint.h>
#include "../common/protocol.h"
/* HAL integration scaffold. value: 0 unknown, 1 inspection pending, 2 staff-confirmed, 3 hold. */
static uint16_t rack_state;
void rack_button_inspection(void) { rack_state=1; }
void rack_button_ready(void) { rack_state=2; }
void rack_button_hold(void) { rack_state=3; }
cupcycle_frame_t rack_status(uint16_t seq, uint32_t now) {
    cupcycle_frame_t f={1,3,2,seq,now,rack_state}; return f;
}
