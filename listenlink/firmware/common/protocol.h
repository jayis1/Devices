#include <stdint.h>
#include <stddef.h>

#define LL_FRAME_SIZE 13u
#define LL_VERSION 1u
int ll_encode(uint8_t out[LL_FRAME_SIZE], uint16_t node, uint32_t seq, uint8_t flags, uint16_t mv);
int ll_decode(const uint8_t *in, size_t len, uint16_t *node, uint32_t *seq, uint8_t *flags, uint16_t *mv);
