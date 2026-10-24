#include "protocol.h"
#include <assert.h>
#include <string.h>
int main(void) {
    const uint8_t text[]="123456789"; assert(pl_crc16(text,9)==0x29B1u);
    uint8_t p[]={0x01,0x02,0x03}, out[PL_MAX_FRAME]; size_t n=0;
    assert(pl_encode(PL_MASS,0x1234,17,0,p,sizeof p,out,sizeof out,&n)==0);
    uint8_t kind,flags,plen; uint16_t node,seq; const uint8_t *decoded;
    assert(pl_decode(out,n,&kind,&node,&seq,&flags,&decoded,&plen)==0);
    assert(kind==PL_MASS && node==0x1234 && seq==17 && plen==3 && memcmp(decoded,p,3)==0);
    out[8]^=1; assert(pl_decode(out,n,&kind,&node,&seq,&flags,&decoded,&plen)==-4);
    assert(pl_encode(1,1,1,0,p,33,out,sizeof out,&n)==-1);
    return 0;
}
