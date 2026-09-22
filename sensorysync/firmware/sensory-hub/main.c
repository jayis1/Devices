/* Portable reference firmware, authored by jayis1. */
#include <stdint.h>
#include "../common/sensorysync_protocol.h"
static uint32_t last_seq[65];
int main(void) { ss_frame_t frame={0}; frame.version=SS_PROTOCOL_VERSION; frame.node_id=16; frame.seq=1; frame.epoch=1; if (ss_validate(&frame,last_seq[frame.node_id],1)) last_seq[frame.node_id]=frame.seq; return 0; }
