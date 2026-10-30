#ifndef SHADESHARE_PROTOCOL_H
#define SHADESHARE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define SS_FRAME_LEN 14u
#define SS_VERSION 1u
uint16_t ss_crc16(const uint8_t *data, size_t len);
int ss_encode(uint8_t out[SS_FRAME_LEN], uint32_t id, uint16_t seq, uint8_t kind, int16_t value, uint16_t mv);
int ss_decode(const uint8_t *in, size_t len, uint32_t *id, uint16_t *seq, uint8_t *kind, int16_t *value, uint16_t *mv);
#endif
