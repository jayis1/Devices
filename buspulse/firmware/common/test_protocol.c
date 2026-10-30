#include <assert.h>
#include "protocol.h"
int main(void){uint8_t b[BP_FRAME_LEN],n=0,f=0;uint16_t s=0,v=0;uint32_t t=0;int16_t c=0;assert(bp_encode(7,123,456789,1,3300,-125,b)==0);assert(bp_decode(b,sizeof b,&n,&s,&t,&f,&v,&c)==0);assert(n==7&&s==123&&t==456789&&f==1&&v==3300&&c==-125);b[10]^=1;assert(bp_decode(b,sizeof b,0,0,0,0,0,0)<0);assert(bp_decode(b,17,0,0,0,0,0,0)<0);return 0;}
