#include <stdbool.h>
#include <stdint.h>
#include "../common/protocol.h"
extern bool reed_is_open(void);
extern void emit_event(const cl_event_t *event);

void shelf_poll(const char *node_id, uint32_t *sequence, bool *last, uint32_t now_ms) {
    bool open = reed_is_open();
    if (open == *last) return;
    *last = open;
    cl_event_t e = { CLAIMLOOP_VERSION, {0}, (*sequence)++, CL_SHELF_DOOR, open ? 1 : 0, now_ms };
    for (unsigned i = 0; node_id[i] && i < sizeof e.node_id - 1; ++i) e.node_id[i] = node_id[i];
    emit_event(&e);
}
