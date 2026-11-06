#ifndef POWERBANKLOOP_PROTOCOL_H
#define POWERBANKLOOP_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define PBL_MAX_PAYLOAD 48u
#define PBL_MAX_FRAME (12u + PBL_MAX_PAYLOAD)
typedef struct { uint8_t node; uint32_t seq; uint8_t kind; uint8_t len; uint8_t payload[PBL_MAX_PAYLOAD]; } pbl_frame_t;
/* Returns encoded length, or 0 for invalid input/capacity. CRC is accidental-error detection only. */
size_t pbl_encode(const pbl_frame_t *f, uint8_t *out, size_t cap);
/* Returns 1 on valid frame, 0 on malformed/CRC failure. */
int pbl_decode(const uint8_t *data, size_t len, pbl_frame_t *out);
#endif
