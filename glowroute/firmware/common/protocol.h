#ifndef GLOWROUTE_PROTOCOL_H
#define GLOWROUTE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define GR_FRAME_SIZE 10
typedef struct { uint8_t node; uint32_t seq; uint16_t lux_milli; } gr_reading;
uint16_t gr_crc16(const uint8_t *data, size_t len);
int gr_encode(const gr_reading *r, uint8_t out[GR_FRAME_SIZE]);
int gr_decode(const uint8_t *in, size_t len, gr_reading *out);
#endif
