/* Workshop Hub portable state machine — authored by jayis1. */
#include <stdint.h>
#include "../common/workshopsync_protocol.h"
typedef struct { uint32_t last_seq; uint32_t epoch; uint8_t radio_online; } hub_state_t;
int hub_accept(hub_state_t *s,const ws_frame_t *f,uint32_t wire_len) { if(!s || !ws_frame_valid(f,wire_len,s->last_seq,s->epoch)) return -1; s->last_seq=f->seq; return 0; }
int main(void) { hub_state_t state={0,1,0}; (void)state; for(;;) { /* vendor HAL: receive/store/render advisory only */ } }
