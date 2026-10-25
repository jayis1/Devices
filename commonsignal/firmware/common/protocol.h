#ifndef COMMONSIGNAL_PROTOCOL_H
#define COMMONSIGNAL_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define CS_MAX_PAYLOAD 96
#define CS_HEADER_SIZE 13
#define CS_FRAME_MAX (CS_HEADER_SIZE + CS_MAX_PAYLOAD + 2)
enum { CS_HEARTBEAT=1, CS_ROUTE=2, CS_EQUIPMENT=3, CS_CLIMATE=4, CS_FAULT=5 };
uint16_t cs_crc16(const uint8_t *data, size_t len);
size_t cs_encode(uint8_t *out, size_t cap, uint8_t type, uint16_t node, uint16_t seq, uint32_t uptime_s, const uint8_t *payload, uint8_t len);
int cs_decode(const uint8_t *in, size_t len, uint8_t *type, uint16_t *node, uint16_t *seq, uint32_t *uptime_s, const uint8_t **payload, uint8_t *payload_len);
#endif
