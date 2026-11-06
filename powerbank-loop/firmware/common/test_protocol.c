#include "protocol.h"
#include <assert.h>
#include <string.h>
int main(void){uint8_t b[PBL_MAX_FRAME];pbl_frame_t a={.node=7,.seq=0x12345678u,.kind=2,.len=3,.payload={1,2,3}},z={0};size_t n=pbl_encode(&a,b,sizeof b);assert(n==15);assert(pbl_decode(b,n,&z));assert(z.node==a.node&&z.seq==a.seq&&z.kind==a.kind&&z.len==3&&!memcmp(a.payload,z.payload,3));b[11]^=1;assert(!pbl_decode(b,n,&z));assert(!pbl_decode(b,n-1,&z));a.len=PBL_MAX_PAYLOAD+1;assert(!pbl_encode(&a,b,sizeof b));return 0;}
