#include <stdbool.h>
#include <stdint.h>
#include "../common/protocol.h"
/* Hardware-independent policy: absent inspection can never imply readiness. */
cp_state_t entry_state(bool door_open, bool staff_checked, bool sample_valid) {
    if (!sample_valid) return CP_UNKNOWN;
    if (door_open || !staff_checked) return CP_CHECK_REQUIRED;
    return CP_OPERATOR_CHECKED;
}
#ifdef CAREPATH_STANDALONE
#include <stdio.h>
int main(void) {
    if (entry_state(false, false, true) != CP_CHECK_REQUIRED) return 1;
    if (entry_state(false, true, true) != CP_OPERATOR_CHECKED) return 2;
    if (entry_state(true, true, true) != CP_CHECK_REQUIRED) return 3;
    if (entry_state(false, true, false) != CP_UNKNOWN) return 4;
    puts("entry policy tests passed"); return 0;
}
#endif
