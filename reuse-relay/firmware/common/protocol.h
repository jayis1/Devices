#ifndef REUSE_RELAY_PROTOCOL_H
#define REUSE_RELAY_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define RR_FRAME_SIZE 19u
#define RR_VERSION 1u
typedef enum { RR_WEIGHT_SAMPLE=1, RR_CONTACT=2, RR_STAFF_CONFIRM=3, RR_HEARTBEAT=4 } rr_type_t;
typedef struct { uint8_t type; uint32_t node_id, sequence; int32_t value_mg; uint8_t flags; } rr_message_t;
uint16_t rr_crc16(const uint8_t *data, size_t len);
int rr_encode(const rr_message_t *msg, uint8_t out[RR_FRAME_SIZE]);
int rr_decode(const uint8_t *in, size_t len, rr_message_t *msg);
#endif
