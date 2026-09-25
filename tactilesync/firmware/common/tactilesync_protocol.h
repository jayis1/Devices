#ifndef TACTILESYNC_PROTOCOL_H
#define TACTILESYNC_PROTOCOL_H
#include <stdint.h>
#define TS_PROTOCOL_VERSION 1u
#define TS_MAX_PAYLOAD 32u
typedef enum { TS_EVENT_ZONE=1, TS_EVENT_DOOR=2, TS_EVENT_APPLIANCE=3, TS_EVENT_ACK=4 } ts_event_type_t;
typedef struct { uint8_t version, type, source_id, key_id; uint32_t sequence; uint8_t payload_len; uint8_t payload[TS_MAX_PAYLOAD]; uint32_t mic; } ts_frame_t;
uint32_t ts_frame_mic(const ts_frame_t *frame, const uint8_t key[16]);
int ts_frame_validate(const ts_frame_t *frame, uint32_t last_sequence, const uint8_t key[16]);
#endif
