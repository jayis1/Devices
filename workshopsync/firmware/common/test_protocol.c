/* Protocol test — authored by jayis1. */
#include <assert.h>
#include <string.h>
#include "workshopsync_protocol.h"
int main(void) { ws_frame_t frame; memset(&frame,0,sizeof(frame)); frame.version=1; frame.type=WS_TELEMETRY; frame.node_id=16; frame.seq=2; frame.epoch=7; frame.length=2; frame.payload[0]=3; frame.payload[1]=4; frame.crc32c=ws_crc32c((const uint8_t*)&frame,17); assert(ws_frame_valid(&frame,21,1,7)); assert(!ws_frame_valid(&frame,21,2,7)); frame.payload[1]=5; assert(!ws_frame_valid(&frame,21,1,7)); return 0; }
