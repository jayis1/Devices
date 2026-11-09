#ifndef MENDMATE_PROTOCOL_H
#define MENDMATE_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MM_VERSION 1u
#define MM_MAX_PAYLOAD 32u
typedef struct { uint8_t node; uint32_t seq; uint8_t kind; uint8_t length; uint8_t payload[MM_MAX_PAYLOAD]; } mm_packet_t;
/* Wire: version,node,kind,length,seq little-endian,payload,CRC-16/CCITT-FALSE LE. */
size_t mm_encode(const mm_packet_t *packet, uint8_t *out, size_t capacity);
bool mm_decode(const uint8_t *bytes, size_t length, mm_packet_t *packet);
#endif
