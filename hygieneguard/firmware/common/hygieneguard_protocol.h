/* HygieneGuard reference protocol. Author: jayis1. */
#ifndef HYGIENEGUARD_PROTOCOL_H
#define HYGIENEGUARD_PROTOCOL_H
#include <stdint.h>
#define HG_VERSION 1u
#define HG_MAX_PAYLOAD 64u
typedef enum { HG_TELEMETRY=1, HG_EVENT=2, HG_HEALTH=3, HG_COMMAND=4 } HGType;
typedef struct __attribute__((packed)) {
  uint8_t version, type, node_id, payload_len;
  uint32_t epoch, sequence;
  uint8_t payload[HG_MAX_PAYLOAD];
  uint32_t crc32c;
} HGFrame;
typedef struct { uint32_t flow_ml; uint16_t soap_g; int16_t temperature_c_x100; uint16_t flags; } HGTelemetry;
uint32_t hg_crc32c(const uint8_t *data, uint32_t size);
int hg_frame_valid(const HGFrame *frame);
#endif
