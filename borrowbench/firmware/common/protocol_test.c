#include "protocol.h"
#include <assert.h>
int main(void) { bb_event_t e = {0}; assert(!bb_event_valid(0)); e.version=BB_VERSION; e.kind=1; e.payload_len=BB_MAX_PAYLOAD; assert(bb_event_valid(&e)); e.payload_len=BB_MAX_PAYLOAD+1; assert(!bb_event_valid(&e)); e.payload_len=0; e.kind=4; assert(!bb_event_valid(&e)); e.kind=2; e.version=2; assert(!bb_event_valid(&e)); }
