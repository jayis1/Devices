#include <stdint.h>
#ifndef CLAIMLOOP_PROTOCOL_H
#define CLAIMLOOP_PROTOCOL_H
#define CLAIMLOOP_VERSION 1u
#define CLAIMLOOP_MAX_PAYLOAD 256u
typedef enum { CL_TRAY_WEIGHT_G = 1, CL_SHELF_DOOR = 2, CL_STAFF_INTAKE = 3 } cl_kind_t;
typedef struct {
    uint8_t version;
    char node_id[24];
    uint32_t sequence;
    uint8_t kind;
    int32_t value;
    uint32_t monotonic_ms;
} cl_event_t;
/* Serialize over an authenticated venue-local transport. Reject oversized,
 * unknown-version, duplicate (node_id, sequence), and out-of-range events. */
#endif
