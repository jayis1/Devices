#ifndef RAINBARREL_PROTOCOL_H
#define RAINBARREL_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define RB_VERSION 1u
#define RB_MAX_PAYLOAD 24u
#define RB_MAX_FRAME (18u + RB_MAX_PAYLOAD)
typedef struct { uint8_t type,node_id; uint32_t boot_id,sequence,uptime_s; uint8_t payload_len,payload[RB_MAX_PAYLOAD]; } rb_packet_t;
/* Wire: version,type,node,payload_len,boot_id LE,seq LE,uptime LE,payload,CRC16-CCITT LE. */
size_t rb_encode(const rb_packet_t *packet,uint8_t *out,size_t capacity);
int rb_decode(const uint8_t *data,size_t length,rb_packet_t *out);
#endif
