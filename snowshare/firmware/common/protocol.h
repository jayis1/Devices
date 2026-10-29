#include <stddef.h>
#include <stdint.h>

#define SS_MAX_PAYLOAD 512u
#define SS_MAX_FRAME (SS_MAX_PAYLOAD + 6u)
int ss_encode(uint8_t type, const uint8_t *payload, uint16_t length, uint8_t *out, size_t capacity, size_t *written);
int ss_decode(const uint8_t *frame, size_t length, uint8_t *type, uint8_t *payload, size_t capacity, uint16_t *payload_length);
