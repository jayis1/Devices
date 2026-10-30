#include "protocol.h"
#include <assert.h>
int main(void){uint8_t out[CR_FRAME_MAX],p[]={0x01,0x02};size_t n=0;assert(cr_encode(1,0x1234,7,p,2,out,sizeof out,&n));assert(n==12&&cr_validate(out,n));assert(out[2]==0x12&&out[4]==0&&out[7]==7);out[9]^=1;assert(!cr_validate(out,n));assert(!cr_encode(1,1,1,p,2,out,4,&n));return 0;}
