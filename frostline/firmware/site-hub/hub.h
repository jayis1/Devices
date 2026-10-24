#ifndef FROSTLINE_HUB_H
#define FROSTLINE_HUB_H
#include "../common/protocol.h"
/* Hub validates frames, deduplicates sequence numbers, buffers, then forwards. */
int frostline_hub_accept(const uint8_t *frame,size_t n,fl_telemetry_t *out);
#endif
