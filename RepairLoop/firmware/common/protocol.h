#ifndef REPAIRLOOP_PROTOCOL_H
#define REPAIRLOOP_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define RL_VERSION 1u
#define RL_MAX_PAYLOAD 48u
#define RL_HEADER_SIZE 7u
#define RL_CRC_SIZE 2u
#define RL_MAX_FRAME (RL_HEADER_SIZE + RL_MAX_PAYLOAD + RL_CRC_SIZE)
typedef struct { uint8_t version, node_type; uint16_t node_id, sequence; uint8_t payload_len; uint8_t payload[RL_MAX_PAYLOAD]; } rl_frame_t;
uint16_t rl_crc16(const uint8_t *data, size_t len);
int rl_encode(const rl_frame_t *frame, uint8_t *out, size_t cap, size_t *written);
int rl_decode(const uint8_t *data, size_t len, rl_frame_t *out);
#endif
