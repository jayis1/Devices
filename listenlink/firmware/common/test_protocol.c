#include "protocol.h"
#include <assert.h>
#include <string.h>
int main(void){uint8_t b[LL_FRAME_SIZE];uint16_t id,mv;uint32_t s;uint8_t f;
 assert(ll_encode(b,17,42,1,2900));assert(ll_decode(b,sizeof b,&id,&s,&f,&mv));assert(id==17&&s==42&&f==1&&mv==2900);
 assert(!ll_decode(b,sizeof b-1,&id,&s,&f,&mv));b[6]^=1;assert(!ll_decode(b,sizeof b,&id,&s,&f,&mv));return 0;}
