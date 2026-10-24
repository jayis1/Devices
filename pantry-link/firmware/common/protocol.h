#ifndef PANTRY_LINK_PROTOCOL_H
#define PANTRY_LINK_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define PL_VERSION 1u
#define PL_MAX_PAYLOAD 32u
#define PL_HEADER_SIZE 8u
#define PL_CRC_SIZE 2u
#define PL_MAX_FRAME (PL_HEADER_SIZE + PL_MAX_PAYLOAD + PL_CRC_SIZE)
enum pl_kind { PL_TEMPERATURE=1, PL_MASS=2, PL_DOOR=3, PL_INTAKE=4, PL_HEARTBEAT=5 };
/* Wire: version, kind, node_id LE16, seq LE16, payload_len, flags, payload, CRC LE16. */
uint16_t pl_crc16(const uint8_t *data, size_t length);
int pl_encode(uint8_t kind, uint16_t node_id, uint16_t seq, uint8_t flags,
              const uint8_t *payload, uint8_t payload_len,
              uint8_t *out, size_t capacity, size_t *written);
int pl_decode(const uint8_t *frame, size_t length, uint8_t *kind,
              uint16_t *node_id, uint16_t *seq, uint8_t *flags,
              const uint8_t **payload, uint8_t *payload_len);
#endif
