#ifndef DRAINWATCH_PROTOCOL_H
#define DRAINWATCH_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define DW_FRAME_SIZE 16u
#define DW_VERSION 1u
#define DW_KIND_INLET 1u
#define DW_KIND_RAIN 2u
uint16_t dw_crc16(const uint8_t *data, size_t len);
int dw_encode(uint8_t out[DW_FRAME_SIZE], uint8_t kind, uint32_t node_id, uint32_t sequence, int32_t value);
int dw_decode(const uint8_t *frame, size_t len, uint8_t *kind, uint32_t *node_id, uint32_t *sequence, int32_t *value);
#endif
