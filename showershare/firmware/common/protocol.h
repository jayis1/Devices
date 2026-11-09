#include <stdint.h>
#ifndef SHOWERSHARE_PROTOCOL_H
#define SHOWERSHARE_PROTOCOL_H
#define SS_VERSION 1u
#define SS_MAX_JSON_BYTES 512u
typedef struct { uint8_t version; uint32_t sequence; int32_t value; } ss_event_t;
static inline int ss_accept_sequence(uint32_t incoming, uint32_t previous) {
    return (int32_t)(incoming - previous) > 0;
}
#endif
