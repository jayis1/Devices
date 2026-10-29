#ifndef FILTERSHARE_PROTOCOL_H
#define FILTERSHARE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define FS_FRAME_LEN 26u
#define FS_MAGIC 0x4653u
#define FS_VERSION 1u
uint16_t fs_crc16(const uint8_t *p, size_t n);
int fs_encode(uint8_t out[FS_FRAME_LEN], uint8_t type, uint32_t node, uint32_t seq,
              uint32_t uptime, int32_t a, int32_t b);
int fs_decode(const uint8_t *in, size_t n, uint8_t *type, uint32_t *node,
              uint32_t *seq, uint32_t *uptime, int32_t *a, int32_t *b);
#endif
