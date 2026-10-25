#include "protocol.h"
#include <assert.h>
int main(void){struct rr_frame a={RR_VERSION,1,42,7,1234,8,0,0},b;uint8_t bytes[RR_FRAME_SIZE];assert(rr_encode(&a,bytes)==0);assert(rr_decode(bytes,&b)==0);assert(b.node_id==42&&b.sequence==7&&b.value_a==8);bytes[3]^=1;assert(rr_decode(bytes,&b)<0);return 0;}
