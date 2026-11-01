#ifndef NEIGHBOR_NEST_PROTOCOL_H
#define NEIGHBOR_NEST_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define NN_VERSION 1u
#define NN_MAX_PAYLOAD 20u
#define NN_HEADER_SIZE 7u
#define NN_MAX_FRAME (NN_HEADER_SIZE + NN_MAX_PAYLOAD + 2u)
uint16_t nn_crc16(const uint8_t *data, size_t length);
int nn_encode(uint8_t *out, size_t capacity, uint16_t node_id,
              uint16_t sequence, uint8_t kind,
              const uint8_t *payload, uint8_t payload_len, size_t *written);
int nn_validate(const uint8_t *frame, size_t length);
#endif
