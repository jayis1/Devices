#ifndef BUSPULSE_PROTOCOL_H
#define BUSPULSE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define BP_FRAME_LEN 18u
uint16_t bp_crc16(const uint8_t *data,size_t len);
int bp_encode(uint8_t node,uint16_t seq,uint32_t timestamp,uint8_t flags,uint16_t value_mv,int16_t temp_centi,uint8_t out[BP_FRAME_LEN]);
int bp_decode(const uint8_t *in,size_t len,uint8_t *node,uint16_t *seq,uint32_t *timestamp,uint8_t *flags,uint16_t *value_mv,int16_t *temp_centi);
#endif
