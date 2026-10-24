#ifndef TOOLSHARE_PROTOCOL_H
#define TOOLSHARE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define TS_FRAME_SIZE 16u
#define TS_VERSION 1u
typedef struct { uint8_t version, type; uint16_t node_id; uint32_t sequence; int32_t value; uint16_t flags; } ts_message_t;
uint16_t ts_crc16(const uint8_t *data, size_t length);
int ts_encode(const ts_message_t *message, uint8_t out[TS_FRAME_SIZE]);
int ts_decode(const uint8_t *frame, size_t length, ts_message_t *message);
#endif
