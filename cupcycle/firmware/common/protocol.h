#ifndef CUPCYCLE_PROTOCOL_H
#define CUPCYCLE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define CUPCYCLE_FRAME_LEN 13u
typedef struct { uint8_t version,node,kind; uint16_t sequence; uint32_t unix_s; uint16_t value; } cupcycle_frame_t;
uint16_t cupcycle_crc16(const uint8_t *data, size_t length);
int cupcycle_encode(const cupcycle_frame_t *frame, uint8_t out[CUPCYCLE_FRAME_LEN]);
int cupcycle_decode(const uint8_t *in, size_t length, cupcycle_frame_t *frame);
#endif
