#ifndef MARKETCHILL_PROTOCOL_H
#define MARKETCHILL_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define MC_FRAME_SIZE 9u
#define MC_VERSION 1u
#define MC_PROBE 1u
#define MC_DOOR 2u
#define MC_TEMP_INVALID INT16_MIN
typedef struct { uint8_t version, kind; uint16_t sequence; int16_t temp_centi_c; uint8_t door_state; uint16_t battery_mv; } mc_frame_t;
/* Encode/decode exact 9-byte little-endian v1 payload. Return 0 on success. */
int mc_encode(const mc_frame_t *frame, uint8_t out[MC_FRAME_SIZE]);
int mc_decode(const uint8_t *data, size_t length, mc_frame_t *out);
#endif
