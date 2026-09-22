/* Authored by jayis1. */
#ifndef SENSORYSYNC_PROTOCOL_H
#define SENSORYSYNC_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define SS_PROTOCOL_VERSION 1u
#define SS_MAX_PAYLOAD 64u
typedef struct { uint8_t version,node_id,type,flags; uint32_t epoch,seq; uint16_t payload_len; uint8_t payload[SS_MAX_PAYLOAD]; uint32_t crc32c; } ss_frame_t;
uint32_t ss_crc32c(const uint8_t *data, size_t length);
bool ss_validate(const ss_frame_t *frame, uint32_t last_seq, uint32_t now_epoch);
#endif
