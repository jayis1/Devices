#ifndef PAWSTATION_PROTOCOL_H
#define PAWSTATION_PROTOCOL_H
#include <stdint.h>
#define PAWSTATION_PROTOCOL_VERSION 1u
#define PAWSTATION_MAX_FRAME 96u
typedef enum { PS_BAG_LEVEL=1, PS_BIN_FILL=2, PS_LID_OPEN=3, PS_SERVICE_CONFIRMED=4 } ps_kind_t;
typedef struct { uint8_t version; char node_id[20]; uint32_t seq; uint8_t kind; int32_t value; uint32_t utc_ms; } ps_event_t;
/* Transport adapters must bound input to PAWSTATION_MAX_FRAME and validate fields. */
#endif
