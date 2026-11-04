#ifndef REPAIR_CAFE_PROTOCOL_H
#define REPAIR_CAFE_PROTOCOL_H
#include <stdint.h>
#define RC_VERSION 1u
#define RC_MAX_PAYLOAD 48u
/* Wire-independent reference event contract; BLE GATT framing is not implemented. */
typedef enum { RC_TICKET_CREATED=1, RC_BENCH_STATE=2, RC_TICKET_STATE=3 } rc_event_type_t;
typedef enum { RC_BENCH_UNKNOWN=0, RC_BENCH_READY=1, RC_BENCH_BUSY=2, RC_BENCH_PAUSED=3 } rc_bench_state_t;
typedef struct {
  uint8_t version, event_type, node_id, state;
  uint32_t sequence;
  uint16_t ticket_id; /* random event-scoped pseudonym; zero when not applicable */
  uint16_t crc16; /* CRC-16/CCITT-FALSE over preceding bytes in serialized frame */
} rc_event_t;
/* Reject mismatched version, invalid type/state, zero node/sequence and repeated seq at receiver. */
#endif
