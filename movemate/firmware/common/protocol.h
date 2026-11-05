#ifndef MOVEMATE_PROTOCOL_H
#define MOVEMATE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define MM_MAX_PAYLOAD 128u
typedef struct { uint8_t version; uint8_t type; uint16_t length; uint32_t sequence; uint8_t payload[MM_MAX_PAYLOAD]; } mm_message;
int mm_encode(const mm_message *m, uint8_t *out, size_t cap, size_t *written);
int mm_decode(const uint8_t *in, size_t len, mm_message *out);
#endif
