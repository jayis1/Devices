#include <stdint.h>
#ifndef CACHEREADY_PROTOCOL_H
#define CACHEREADY_PROTOCOL_H
#define CR_VERSION 1u
#define CR_MAX_PAYLOAD 32u
#define CR_HEADER_LEN 8u
#define CR_FRAME_MAX (CR_HEADER_LEN + CR_MAX_PAYLOAD + 2u)
typedef struct { uint8_t version, node_id, type, seq; uint32_t timestamp_s; uint8_t payload[CR_MAX_PAYLOAD]; uint8_t payload_len; } cr_message_t;
uint16_t cr_crc16(const uint8_t *data, uint16_t length);
int cr_encode(const cr_message_t *msg, uint8_t *out, uint16_t capacity);
int cr_decode(const uint8_t *buf, uint16_t length, cr_message_t *msg);
#endif
