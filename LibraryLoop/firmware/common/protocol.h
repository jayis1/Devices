#ifndef LIBRARYLOOP_PROTOCOL_H
#define LIBRARYLOOP_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define LL_FRAME_SIZE 20u
#define LL_PROTOCOL_VERSION 1u
/* Wire offsets in protocol.md. Bytes are serialized explicitly, never cast to a struct. */
typedef struct {
    uint8_t kind;
    uint8_t node_id;
    uint16_t sequence;
    uint16_t occupied_mask;
    uint16_t valid_mask;
    uint32_t uptime_s;
} ll_telemetry_t;
uint16_t ll_crc16(const uint8_t *data, size_t length);
int ll_encode(uint8_t out[LL_FRAME_SIZE], const ll_telemetry_t *message);
int ll_decode(ll_telemetry_t *message, const uint8_t *frame, size_t length);
#endif
