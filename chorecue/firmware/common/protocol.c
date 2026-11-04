#include "protocol.h"

int cc_validate(cc_event event) {
    switch (event.kind) {
    case CC_TASK_DONE: return event.value == 0 || event.value == 1;
    case CC_SUPPLY_G: return event.value >= 0 && event.value <= 100000;
    case CC_AVAILABILITY: return event.value >= 0 && event.value <= 2;
    default: return 0;
    }
}
