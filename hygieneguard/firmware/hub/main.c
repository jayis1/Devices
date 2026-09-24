/* Hygiene Hub portable state reference. Author: jayis1. */
#include "../common/hygieneguard_protocol.h"
static uint32_t last_sequence[256];
int hub_accept(const HGFrame *frame) {
  if (!hg_frame_valid(frame) || frame->sequence <= last_sequence[frame->node_id]) return 0;
  last_sequence[frame->node_id] = frame->sequence;
  return 1;
}
int main(void) { return 0; }
