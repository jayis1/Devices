#include <stdbool.h>
#include <stdint.h>
#include "../common/protocol.h"
/* Millimetres only; threshold is site-configured, never a compliance claim. */
cp_state_t approach_state(uint16_t clearance_mm, uint16_t minimum_mm, bool valid) {
    if (!valid || clearance_mm > 5000u || minimum_mm == 0u) return CP_UNKNOWN;
    return clearance_mm >= minimum_mm ? CP_OPERATOR_CHECKED : CP_CHECK_REQUIRED;
}
#ifdef CAREPATH_STANDALONE
#include <stdio.h>
int main(void) {
    if (approach_state(1000, 900, true) != CP_OPERATOR_CHECKED) return 1;
    if (approach_state(800, 900, true) != CP_CHECK_REQUIRED) return 2;
    if (approach_state(6000, 900, true) != CP_UNKNOWN) return 3;
    puts("approach policy tests passed"); return 0;
}
#endif
