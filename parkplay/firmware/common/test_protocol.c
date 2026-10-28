#include "protocol.h"
#include <assert.h>
#include <string.h>
int main(void){ uint8_t b[PP_FRAME_SIZE],t; uint32_t id,s; int32_t v;
 assert(pp_encode(b,2,0x12345678u,42u,-1234)==0); assert(pp_decode(b,sizeof b,&t,&id,&s,&v)==0);
 assert(t==2 && id==0x12345678u && s==42u && v==-1234); b[3]^=1; assert(pp_decode(b,sizeof b,&t,&id,&s,&v)==-2);
 assert(pp_decode(b,15,&t,&id,&s,&v)==-1); assert(pp_encode(b,9,1,1,0)==-1); return 0; }
