#include "protocol.h"
#include <assert.h>
int main(void){uint8_t b[MM_FRAME_SIZE];mm_frame_t a={2,0x12345678u,9,4200,-123,1},z;
 assert(mm_encode(&a,b));assert(mm_decode(b,sizeof b,&z));assert(z.node_id==a.node_id&&z.sequence==a.sequence&&z.value==a.value&&z.status==a.status);
 assert(!mm_decode(b,sizeof b-1,&z));b[8]^=1;assert(!mm_decode(b,sizeof b,&z));return 0;}
