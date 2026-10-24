#include <stdint.h>
/* Hub policy: unknown/stale measurements stay unknown; no automatic food disposition. */
int sample_is_current(uint32_t now_s, uint32_t sample_s) {
    return now_s >= sample_s && (now_s - sample_s) <= 300u;
}
