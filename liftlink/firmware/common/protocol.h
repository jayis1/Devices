#ifndef LIFTLINK_PROTOCOL_H
#define LIFTLINK_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define LL_FRAME_LEN 16u
#define LL_VERSION 1u
enum ll_type { LL_HEARTBEAT=1, LL_LIFT_SAMPLE=2, LL_USER_REPORT=3, LL_STAFF_ACK=4 };
enum ll_status { LL_UNKNOWN=0, LL_LIKELY_AVAILABLE=1, LL_LIKELY_UNAVAILABLE=2, LL_STAFF_UNAVAILABLE=3, LL_STAFF_AVAILABLE=4 };
typedef struct { uint8_t version; uint16_t node_id; uint32_t sequence; uint8_t type; uint8_t status; uint8_t confidence; } ll_event_t;
uint16_t ll_crc16(const uint8_t *data, size_t len);
int ll_encode(const ll_event_t *event, uint32_t uptime_ms, uint8_t out[LL_FRAME_LEN]);
int ll_decode(const uint8_t *in, size_t len, ll_event_t *event, uint32_t *uptime_ms);
#endif
