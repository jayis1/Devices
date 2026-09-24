/* HygieneGuard reference protocol. Author: jayis1. */
#include "hygieneguard_protocol.h"
uint32_t hg_crc32c(const uint8_t *data, uint32_t size) {
  uint32_t crc = 0xffffffffu;
  for (uint32_t i = 0; i < size; ++i) { crc ^= data[i]; for (uint8_t b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0x82f63b78u & (uint32_t)-(int32_t)(crc & 1u)); }
  return ~crc;
}
int hg_frame_valid(const HGFrame *frame) {
  if (!frame || frame->version != HG_VERSION || frame->payload_len > HG_MAX_PAYLOAD) return 0;
  return hg_crc32c((const uint8_t *)frame, (uint32_t)(12u + frame->payload_len)) == frame->crc32c;
}
