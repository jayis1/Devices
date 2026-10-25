#ifndef SHELTER_SIGNAL_PROTOCOL_H
#define SHELTER_SIGNAL_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define SS_VERSION 1u
#define SS_FRAME_SIZE 20u
typedef enum { SS_LIGHT=1, SS_WATER_CONTACT=2, SS_APPROACH=3, SS_HEARTBEAT=4 } ss_kind_t;
typedef struct { uint8_t version, kind; uint16_t node_id; uint32_t sequence; uint32_t age_s; int32_t value; } ss_message_t;
/* Wire: version,kind,node_id LE,sequence LE,age_s LE,value LE,CRC16 LE,reserved zero. CRC covers bytes 0..15. */
uint16_t ss_crc16(const uint8_t *data, size_t len);
int ss_encode(const ss_message_t *msg, uint8_t out[SS_FRAME_SIZE]);
int ss_decode(const uint8_t *frame, size_t len, ss_message_t *out);
#endif
