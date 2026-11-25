#ifndef MEAL_MOSAIC_PROTOCOL_H
#define MEAL_MOSAIC_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define MM_FRAME_SIZE 20u
#define MM_VERSION 1u
#define MM_MAGIC 0x4du
/* Wire offsets are encoded explicitly; never transmit a C struct. */
typedef struct { uint8_t kind; uint32_t node_id, sequence, uptime_ms; int16_t value; uint8_t status; } mm_frame_t;
uint8_t mm_crc8(const uint8_t *data, size_t len);
int mm_encode(const mm_frame_t *frame, uint8_t out[MM_FRAME_SIZE]);
int mm_decode(const uint8_t *in, size_t len, mm_frame_t *out);
#endif
