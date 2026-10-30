#include <assert.h>
#include <stdint.h>
#include "protocol.h"
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
int main(void){
    uint8_t f[PP_FRAME_OVERHEAD+3]={0x50,0x50,1,1,7,0,1,0,3,0,0x34,0x12,0x56,0,0};
    put16(&f[sizeof f-2],pp_crc16(f,sizeof f-2));
    assert(pp_frame_valid(f,sizeof f));
    f[10]^=1; assert(!pp_frame_valid(f,sizeof f));
    f[10]^=1; assert(!pp_frame_valid(f,sizeof f-1));
    f[2]=2; assert(!pp_frame_valid(f,sizeof f));
    return 0;
}
