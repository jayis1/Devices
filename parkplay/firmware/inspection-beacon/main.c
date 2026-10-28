#include "protocol.h"
int inspection_beacon_make_frame(uint8_t out[PP_FRAME_SIZE], uint32_t id, uint32_t seq, int32_t event) {
  if (event < 1 || event > 3) return -1;
  return pp_encode(out, 3u, id, seq, event);
}
