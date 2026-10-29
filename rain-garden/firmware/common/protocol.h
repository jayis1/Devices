#include <stdint.h>
#include <stddef.h>

#define RG_PROTOCOL_VERSION 1u
#define RG_MAX_PAYLOAD 192u
uint16_t rg_crc16(const uint8_t *data, size_t length);
int rg_valid_value(float value, float low, float high);
