#include <stdbool.h>
#include "../common/protocol.h"
cp_state_t hub_state(bool entry_fresh, bool entry_checked, bool approach_fresh,
                     bool approach_clear, bool operator_confirmed) {
    if (!entry_fresh || !approach_fresh) return CP_UNKNOWN;
    if (!entry_checked || !approach_clear || !operator_confirmed) return CP_CHECK_REQUIRED;
    return CP_OPERATOR_CHECKED;
}
#ifdef CAREPATH_STANDALONE
#include <stdio.h>
int main(void) {
 if (hub_state(false,true,true,true,true)!=CP_UNKNOWN) return 1;
 if (hub_state(true,true,true,true,false)!=CP_CHECK_REQUIRED) return 2;
 if (hub_state(true,true,true,true,true)!=CP_OPERATOR_CHECKED) return 3;
 puts("hub policy tests passed"); return 0;
}
#endif
