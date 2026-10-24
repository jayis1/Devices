#ifndef QUEUEKIND_PROTOCOL_H
#define QUEUEKIND_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define QK_VERSION 1u
#define QK_MAX_PAYLOAD 8u
#define QK_MAX_FRAME 18u
enum { QK_ARRIVAL=1, QK_COMPLETE=2, QK_PAUSE=3, QK_RESUME=4 };
typedef struct { uint8_t type,node; uint16_t seq; uint8_t payload_len; uint8_t payload[QK_MAX_PAYLOAD]; } qk_frame_t;
uint16_t qk_crc16(const uint8_t *data, size_t len);
size_t qk_encode(uint8_t type,uint8_t node,uint16_t seq,const uint8_t *payload,uint8_t payload_len,uint8_t *out,size_t cap);
int qk_decode(const uint8_t *in,size_t len,qk_frame_t *out);
#endif
