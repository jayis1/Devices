#include <stddef.h>
#include <stdint.h>
#define SEED_MAX_PAYLOAD 128u
#define SEED_HEADER 10u
#define SEED_OVERHEAD 12u
uint16_t seed_crc16(const uint8_t *data, size_t len);
int seed_frame_valid(const uint8_t *frame, size_t len);
