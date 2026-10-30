#ifndef COMMUNITYREPAIR_PROTOCOL_H
#define COMMUNITYREPAIR_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define CR_FRAME_MAX 64u
#define CR_VERSION 1u
/* Binary envelope: version,type,node_id(uint16 BE),seq(uint32 BE),payload_len,payload,CRC16-CCITT(BE). */
int cr_encode(uint8_t type, uint16_t node_id, uint32_t seq, const uint8_t *payload, uint8_t payload_len, uint8_t *out, size_t cap, size_t *written);
int cr_validate(const uint8_t *frame, size_t len);
uint16_t cr_crc16(const uint8_t *data, size_t len);
#endif
