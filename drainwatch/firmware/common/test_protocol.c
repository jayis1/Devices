#include "protocol.h"
#include <assert.h>
int main(void){uint8_t f[DW_FRAME_SIZE],k;uint32_t id,s;int32_t v;assert(dw_encode(f,DW_KIND_INLET,0x12345678u,9u,845)==0);assert(dw_decode(f,sizeof f,&k,&id,&s,&v)==0);assert(k==DW_KIND_INLET&&id==0x12345678u&&s==9u&&v==845);assert(dw_decode(f,15,&k,&id,&s,&v)<0);f[2]^=1;assert(dw_decode(f,sizeof f,&k,&id,&s,&v)==-2);return 0;}
