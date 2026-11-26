#ifndef CAPTION_BRIDGE_PROTOCOL_H
#define CAPTION_BRIDGE_PROTOCOL_H
#include <stdbool.h>
#include <stdint.h>
#define CB_PROTOCOL_VERSION 1u
#define CB_MAX_MESSAGE_BYTES 512u
#define CB_PEAK_MAX 1.0f
typedef struct { uint32_t seq; uint32_t uptime_ms; float audio_peak; bool valid; } cb_mic_health_t;
static inline bool cb_seq_is_newer(uint32_t candidate, uint32_t previous) {
    return (int32_t)(candidate - previous) > 0;
}
static inline bool cb_peak_valid(float peak) { return peak >= 0.0f && peak <= CB_PEAK_MAX; }
#endif
