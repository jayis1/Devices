#ifndef POLLINATOR_PROTOCOL_H
#define POLLINATOR_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define PP_MAX_PAYLOAD 64u
#define PP_FRAME_OVERHEAD 12u
uint16_t pp_crc16(const uint8_t *data, size_t len);
int pp_frame_valid(const uint8_t *frame, size_t len);
#endif
