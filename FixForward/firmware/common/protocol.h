#ifndef FIXFORWARD_PROTOCOL_H
#define FIXFORWARD_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define FF_VERSION 1
#define FF_MAX_PAYLOAD 64
typedef struct { uint8_t version,type; uint32_t node_id,sequence; int32_t value; uint8_t unit; } ff_message;
uint16_t ff_crc16(const uint8_t *p,size_t n);
int ff_valid(const ff_message *m);
#endif
