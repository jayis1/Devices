#include "protocol.h"
#include <assert.h>
#include <stdint.h>
int main(void) {
    uint8_t in[3]={1,2,3}, frame[SS_MAX_FRAME], out[SS_MAX_PAYLOAD], type=0;
    uint16_t n=0; size_t written=0;
    assert(ss_encode(7,in,3,frame,sizeof frame,&written)==0);
    assert(ss_decode(frame,written,&type,out,sizeof out,&n)==0 && type==7 && n==3);
    assert(out[0]==1 && out[2]==3);
    frame[4]^=1;
    assert(ss_decode(frame,written,&type,out,sizeof out,&n)!=0);
    return 0;
}
