#include "../common/protocol.h"
#include <assert.h>
int main(void){uint8_t b[GR_FRAME_SIZE];gr_reading a={7,0x12345678,1234},z;assert(gr_encode(&a,b)==0);assert(gr_decode(b,sizeof b,&z)==0);assert(z.node==a.node&&z.seq==a.seq&&z.lux_milli==a.lux_milli);b[2]^=1;assert(gr_decode(b,sizeof b,&z)==-2);assert(gr_decode(b,9,&z)==-1);return 0;}
