#include "protocol.h"
#include <assert.h>
int main(void){uint8_t b[SS_FRAME_LEN];uint32_t id;uint16_t seq,mv;uint8_t kind;int16_t val;assert(ss_encode(b,0x12345678u,42,3,-1234,3010)==0);assert(ss_decode(b,sizeof b,&id,&seq,&kind,&val,&mv)==0);assert(id==0x12345678u&&seq==42&&kind==3&&val==-1234&&mv==3010);b[4]^=1;assert(ss_decode(b,sizeof b,&id,&seq,&kind,&val,&mv)<0);assert(ss_decode(b,12,&id,&seq,&kind,&val,&mv)<0);return 0;}
