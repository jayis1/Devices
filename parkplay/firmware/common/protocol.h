#ifndef PARKPLAY_PROTOCOL_H
#define PARKPLAY_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define PP_FRAME_SIZE 16u
#define PP_VERSION 1u
uint16_t pp_crc16(const uint8_t *data, size_t len);
int pp_encode(uint8_t out[PP_FRAME_SIZE], uint8_t type, uint32_t node_id,
              uint32_t sequence, int32_t value);
int pp_decode(const uint8_t *frame, size_t len, uint8_t *type, uint32_t *node_id,
              uint32_t *sequence, int32_t *value);
#endif
