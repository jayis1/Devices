#ifndef COOKSHARE_PROTOCOL_H
#define COOKSHARE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define CS_FRAME_SIZE 16u
#define CS_MAX_FRAME 32u
#define CS_VERSION 1u
enum { CS_UNKNOWN=0, CS_IDLE=1, CS_ACTIVE=2 };
typedef struct { uint8_t version,type; uint32_t node_id; uint16_t sequence; uint8_t state,flags; uint16_t sample_ma; } cs_frame_t;
uint16_t cs_crc16(const uint8_t *data,size_t len);
int cs_encode(const cs_frame_t *frame,uint8_t *out,size_t cap);
int cs_decode(const uint8_t *data,size_t len,cs_frame_t *out);
#endif
