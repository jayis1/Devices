/* Authored by jayis1. */
#include <assert.h>
#include "sensorysync_protocol.h"
int main(void) { ss_frame_t f={0}; f.version=1; f.payload_len=2; f.seq=2; f.epoch=100; assert(ss_validate(&f,1,100)); assert(!ss_validate(&f,2,100)); f.payload_len=65; assert(!ss_validate(&f,0,100)); return 0; }
