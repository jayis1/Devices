#include <assert.h>
#include <stdint.h>
#include "../firmware/common/protocol.h"
int main(void){uint8_t b[PP_FRAME_SIZE];uint16_t n;uint32_t s,v,u;uint8_t t;assert(pp_encode(b,42,7,PP_FILL,850,123)==PP_FRAME_SIZE);assert(pp_decode(b,sizeof b,&n,&s,&t,&v,&u)==0);assert(n==42&&s==7&&t==PP_FILL&&v==850&&u==123);b[5]^=1;assert(pp_decode(b,sizeof b,0,0,0,0,0)<0);return 0;}
