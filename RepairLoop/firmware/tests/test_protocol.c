#include <assert.h>
#include <string.h>
#include "../common/protocol.h"
int main(void){rl_frame_t a={.version=1,.node_type=2,.node_id=0x1234,.sequence=9,.payload_len=3,.payload={1,2,3}},b={0};uint8_t wire[RL_MAX_FRAME];size_t n=0;assert(rl_encode(&a,wire,sizeof wire,&n)==0);assert(rl_decode(wire,n,&b)==0);assert(b.node_id==a.node_id&&b.sequence==a.sequence&&b.payload_len==3&&!memcmp(a.payload,b.payload,3));wire[n-1]^=1;assert(rl_decode(wire,n,&b)==-2);assert(rl_decode(wire,n-1,&b)==-1);return 0;}
