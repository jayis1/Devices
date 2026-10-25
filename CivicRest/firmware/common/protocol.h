#ifndef CIVICREST_PROTOCOL_H
#define CIVICREST_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define CR_VERSION 1u
#define CR_MAX_PAYLOAD 64u
#define CR_HEADER_SIZE 7u
#define CR_CRC_SIZE 2u
#define CR_MAX_FRAME (CR_HEADER_SIZE + CR_MAX_PAYLOAD + CR_CRC_SIZE)
enum cr_message_type { CR_TELEMETRY=1, CR_STATUS=2, CR_ACK=3 };
/* Wire: version,type,node_id_le16,seq_le16,payload_len,payload,CRC16-CCITT-FALSE LE. */
uint16_t cr_crc16(const uint8_t *data, size_t length);
int cr_encode(uint8_t *out, size_t capacity, uint8_t type, uint16_t node_id,
              uint16_t seq, const uint8_t *payload, uint8_t payload_len, size_t *written);
int cr_decode(const uint8_t *frame, size_t length, uint8_t *type, uint16_t *node_id,
              uint16_t *seq, const uint8_t **payload, uint8_t *payload_len);
#endif
