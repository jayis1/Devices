#include "protocol.h"
#include <assert.h>
int main(void) {
    uint8_t frame[QK_MAX_FRAME]; qk_frame_t decoded;
    size_t n=qk_encode(QK_ARRIVAL,1,1,0,0,frame,sizeof frame);
    assert(n==10); assert(frame[0]==0x51 && frame[1]==0x4b);
    assert(frame[8]==0x3f && frame[9]==0x85);
    assert(qk_decode(frame,n,&decoded)); assert(decoded.type==QK_ARRIVAL && decoded.node==1 && decoded.seq==1);
    frame[0]^=1; assert(!qk_decode(frame,n,&decoded));
    assert(!qk_decode(frame,n-1,&decoded));
    return 0;
}
