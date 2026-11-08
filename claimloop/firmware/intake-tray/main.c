#include <stdbool.h>
#include <stdint.h>
#include "../common/protocol.h"

/* Board HAL integration supplies these functions; this file contains the
 * bounded acquisition/debounce policy, not a standalone flashable image. */
extern int32_t hx711_read_grams(void);
extern bool confirm_button_pressed(void);
extern void emit_event(const cl_event_t *event);

bool intake_sample(const char *node_id, uint32_t seq, uint32_t now_ms) {
    static int32_t previous = -1;
    static unsigned stable = 0;
    int32_t grams = hx711_read_grams();
    if (grams < 0 || grams > 30000) { previous = -1; stable = 0; return false; }
    if (previous >= 0 && grams - previous <= 3 && previous - grams <= 3) stable++;
    else stable = 0;
    previous = grams;
    if (stable < 4 || !confirm_button_pressed()) return false;
    cl_event_t e = { CLAIMLOOP_VERSION, {0}, seq, CL_STAFF_INTAKE, grams, now_ms };
    for (unsigned i = 0; node_id[i] && i < sizeof e.node_id - 1; ++i) e.node_id[i] = node_id[i];
    emit_event(&e);
    stable = 0;
    return true;
}
