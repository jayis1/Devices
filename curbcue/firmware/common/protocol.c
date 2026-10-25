#include "protocol.h"
int cu_event_valid(const cu_event_t *e) {
    return e != 0 && e->kind >= CU_OBSTRUCTION && e->kind <= CU_ACK && e->severity <= 3u;
}
