#include <stdint.h>
#include "../common/protocol.h"

/* Host model only: no microphone driver, radio stack, or audio capture. */
int main(void) {
    cb_mic_health_t sample = { .seq = 1u, .uptime_ms = 0u, .audio_peak = 0.0f, .valid = false };
    return (cb_peak_valid(sample.audio_peak) && sample.seq == 1u) ? 0 : 1;
}
