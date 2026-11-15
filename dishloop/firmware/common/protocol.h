#include <stdint.h>
#include <stddef.h>
#include <math.h>

/* Shared bounded telemetry validation; transport/authentication is platform-specific. */
typedef struct { uint32_t version, sequence; double value; } dishloop_event;
static inline int dishloop_valid(const dishloop_event *e, uint32_t previous) {
    return e && e->version == 1 && e->sequence > previous && isfinite(e->value);
}
