#ifndef BORROWBENCH_PROTOCOL_H
#define BORROWBENCH_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define BB_MAX_PAYLOAD 128u
#define BB_VERSION 1u
typedef struct { uint8_t version; uint8_t kind; uint16_t payload_len; uint32_t seq; uint64_t timestamp_ms; uint8_t node_id[8]; uint8_t payload[BB_MAX_PAYLOAD]; } bb_event_t;
/* Validate structural bounds only; transport authentication and durable replay protection belong to the hub. */
static inline int bb_event_valid(const bb_event_t *e) { return e && e->version == BB_VERSION && e->payload_len <= BB_MAX_PAYLOAD && e->kind >= 1u && e->kind <= 3u; }
#endif
