#include "protocol.h"
/* ESP-IDF sensor acquisition is target-specific and intentionally not stubbed. */
int surface_sentinel_make_frame(uint8_t out[PP_FRAME_SIZE], uint32_t id, uint32_t seq, int32_t centi_celsius) {
  if (centi_celsius < -4000 || centi_celsius > 12500) return -1;
  return pp_encode(out, 2u, id, seq, centi_celsius);
}
