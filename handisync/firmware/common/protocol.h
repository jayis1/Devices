#ifndef HANDISYNC_PROTOCOL_H
#define HANDISYNC_PROTOCOL_H
#include <stdint.h>
#include <stdbool.h>
#define HS_MAX_PAYLOAD 80u
#define HS_TAG_LEN 16u
typedef enum { HS_HEARTBEAT=1, HS_TELEMETRY=2, HS_COMMAND=16, HS_ACK=17, HS_FAULT=18, HS_LEASE=32 } hs_type_t;
typedef struct __attribute__((packed)) { uint8_t version,type; uint32_t node_id,seq,epoch; uint8_t len; uint8_t payload[HS_MAX_PAYLOAD]; uint32_t crc32c; uint8_t tag[HS_TAG_LEN]; } hs_frame_t;
uint32_t hs_crc32c(const uint8_t *data, uint16_t len);
bool hs_validate(const hs_frame_t *f, uint16_t wire_len, uint32_t last_seq);
#endif
