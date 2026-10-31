#include "protocol.h"
#include <assert.h>
int main(void){cs_frame_t a={1,1,0x12345678u,42,CS_ACTIVE,0,315},b={0};uint8_t x[CS_FRAME_SIZE];assert(cs_encode(&a,x,sizeof x)==16);assert(cs_decode(x,16,&b)==0);assert(b.node_id==a.node_id&&b.sequence==42&&b.sample_ma==315);x[10]=255;assert(cs_decode(x,16,&b)<0);assert(cs_encode(&a,x,15)<0);return 0;}
