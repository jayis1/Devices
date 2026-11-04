#include "../common/protocol.h"
/* Portable logic example; ESP-IDF button debounce, BLE and NVS sequence storage are TODO. */
static uint32_t sequence;
rc_event_t bench_changed(uint8_t node_id, rc_bench_state_t state) {
  rc_event_t e = {RC_VERSION, RC_BENCH_STATE, node_id, (uint8_t)state, ++sequence, 0, 0};
  return e; /* No remote actuation; stale state expires at gateway. */
}
