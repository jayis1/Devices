/* WorkshopSync protocol reference — authored by jayis1. */
#ifndef WORKSHOPSYNC_PROTOCOL_H
#define WORKSHOPSYNC_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define WS_PROTOCOL_VERSION 1u
#define WS_MAX_PAYLOAD 96u
typedef enum { WS_HEARTBEAT=1, WS_TELEMETRY=2, WS_READINESS=3, WS_COMMAND=16, WS_ACK=17, WS_FAULT=18 } ws_type_t;
typedef struct __attribute__((packed)) { uint8_t version, type; uint32_t node_id, seq, epoch; uint8_t length; uint8_t payload[WS_MAX_PAYLOAD]; uint32_t crc32c; } ws_frame_t;
uint32_t ws_crc32c(const uint8_t *data, size_t len);
bool ws_frame_valid(const ws_frame_t *frame, size_t wire_len, uint32_t last_seq, uint32_t expected_epoch);
#endif
