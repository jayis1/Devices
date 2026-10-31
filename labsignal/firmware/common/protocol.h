#ifndef LABSIGNAL_PROTOCOL_H
#define LABSIGNAL_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
enum { LABSIGNAL_VERSION = 1, LABSIGNAL_BODY_SIZE = 10, LABSIGNAL_FRAME_SIZE = 12 };
typedef struct { uint8_t kind; uint32_t sequence; int32_t value_milli; } labsignal_message_t;
/* Wire bytes: version, kind, sequence LE32, value_milli LE32, CRC-16/CCITT-FALSE LE16. */
int labsignal_encode(const labsignal_message_t *m, uint8_t out[LABSIGNAL_FRAME_SIZE]);
int labsignal_decode(const uint8_t *in, size_t len, labsignal_message_t *m);
uint16_t labsignal_crc16(const uint8_t *data, size_t len);
#endif
