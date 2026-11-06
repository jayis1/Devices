#ifndef LINEWISE_PROTOCOL_H
#define LINEWISE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define LW_FRAME_LEN 16u
#define LW_VERSION 1u
/* LE framing: version, kind, node_id:u16, seq:u32, value:i32 (scaled units), CRC16. */
uint16_t lw_crc16(const uint8_t *data, size_t len);
int lw_encode(uint8_t out[LW_FRAME_LEN], uint8_t kind, uint16_t node_id, uint32_t seq, int32_t value);
int lw_decode(const uint8_t *in, size_t len, uint8_t *kind, uint16_t *node_id, uint32_t *seq, int32_t *value);
#endif
