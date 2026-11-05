#ifndef REPAIRLOOP_PROTOCOL_H
#define REPAIRLOOP_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define RL_MAX_PAYLOAD 96u
#define RL_VERSION 1u
typedef struct { uint8_t type; uint32_t sequence; uint8_t payload[RL_MAX_PAYLOAD]; uint8_t length; } rl_message;
int rl_encode(const rl_message *msg, uint8_t *out, size_t capacity, size_t *written);
int rl_decode(const uint8_t *data, size_t length, rl_message *out);
#endif
