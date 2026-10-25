#ifndef PARKPULSE_PROTOCOL_H
#define PARKPULSE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define PP_FRAME_SIZE 18u
enum { PP_FLOW=1, PP_LEAK=2, PP_FILL=3, PP_HEALTH=4 };
uint16_t pp_crc16(const uint8_t *data, size_t len);
size_t pp_encode(uint8_t *out, uint16_t node, uint32_t seq, uint8_t type, uint32_t value, uint32_t uptime);
int pp_decode(const uint8_t *in, size_t len, uint16_t *node, uint32_t *seq, uint8_t *type, uint32_t *value, uint32_t *uptime);
#endif
