#ifndef ROUTEREADY_PROTOCOL_H
#define ROUTEREADY_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define RR_VERSION 1u
#define RR_MAX_PAYLOAD 96u
typedef struct { uint8_t kind, length; uint32_t seq; uint8_t payload[RR_MAX_PAYLOAD]; } rr_packet;
int rr_encode(const rr_packet *p, uint8_t *out, size_t cap, size_t *n);
int rr_decode(const uint8_t *in, size_t n, rr_packet *p);
#endif
