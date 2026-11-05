#include <stdint.h>
#ifndef PANTRYPATH_PROTOCOL_H
#define PANTRYPATH_PROTOCOL_H
#define PP_VERSION 1u
#define PP_MAX_PAYLOAD 32u
typedef struct { uint8_t version,node,type,seq; uint32_t epoch_s; uint8_t len; uint8_t payload[PP_MAX_PAYLOAD]; } pp_msg_t;
int pp_encode(const pp_msg_t *m, uint8_t *out, uint16_t cap);
int pp_decode(const uint8_t *in, uint16_t len, pp_msg_t *m);
#endif
