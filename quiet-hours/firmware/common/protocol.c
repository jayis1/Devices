#include <stdint.h>
#include <stdbool.h>

/* Host-portable estimator core. Raw PCM must be discarded by caller. */
uint16_t qh_db10_bucket(const int32_t *samples, uint32_t n) {
    if (!samples || n == 0) return 0;
    uint64_t sum = 0;
    for (uint32_t i = 0; i < n; ++i) {
        int64_t x = samples[i];
        sum += (uint64_t)(x * x);
    }
    /* Deliberately only a relative amplitude bucket; not calibrated dBA. */
    uint64_t mean = sum / n;
    uint16_t bucket = 0;
    while (mean > 1 && bucket < 1200) { mean >>= 1; bucket += 10; }
    return bucket;
}

bool qh_seq_is_newer(uint32_t candidate, uint32_t previous) {
    return (int32_t)(candidate - previous) > 0;
}
