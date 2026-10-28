#ifndef PARCELPAUSE_PROTOCOL_H
#define PARCELPAUSE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define PP_FRAME_LEN 20u
#define PP_VERSION 1u
typedef struct { uint8_t version, node_type; uint32_t node_id, sequence, age_ms; uint16_t value; uint8_t status; } pp_frame_t;
uint16_t pp_crc16(const uint8_t *data, size_t length);
int pp_encode(const pp_frame_t *frame, uint8_t out[PP_FRAME_LEN]);
int pp_decode(const uint8_t *buf, size_t length, pp_frame_t *frame);
#endif
