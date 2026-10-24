#include "../common/protocol.h"
/* Direction counter publishes aggregate deltas only; no persistent trajectory. */
int main(void) { uint8_t frame[LL_FRAME_SIZE]; ll_telemetry_t count={.kind=2u,.node_id=2u,.sequence=0u,.occupied_mask=0u,.valid_mask=0u,.uptime_s=0u}; return ll_encode(frame,&count); }
