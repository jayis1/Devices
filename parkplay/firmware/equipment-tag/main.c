#include "protocol.h"
/* ESP-IDF task body sketch; sensor driver and BLE advertiser require target SDK. */
int equipment_tag_make_frame(uint8_t out[PP_FRAME_SIZE], uint32_t id, uint32_t seq, int32_t motion_delta) {
  if (motion_delta < 0) motion_delta = 0;
  return pp_encode(out, 1u, id, seq, motion_delta);
}
