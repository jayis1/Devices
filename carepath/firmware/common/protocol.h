#ifndef CAREPATH_PROTOCOL_H
#define CAREPATH_PROTOCOL_H
#include <stdbool.h>
#include <stdint.h>
#define CP_PROTOCOL_VERSION 1u
#define CP_MAX_MESSAGE_BYTES 512u
#define CP_FRESHNESS_MS 120000u
typedef enum { CP_UNKNOWN=0, CP_CHECK_REQUIRED=1, CP_OPERATOR_CHECKED=2 } cp_state_t;
typedef struct { uint32_t seq; uint32_t uptime_ms; bool valid; } cp_observation_t;
static inline bool cp_seq_is_newer(uint32_t candidate, uint32_t previous) {
    return (int32_t)(candidate - previous) > 0;
}
#endif
