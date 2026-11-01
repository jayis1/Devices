#include "protocol.h"
#include <assert.h>
int main(void) {
    uint8_t frame[NN_MAX_FRAME], p[]={0x34,0x12}; size_t n=0;
    assert(nn_encode(frame,sizeof frame,0x1234,9,1,p,sizeof p,&n)==0);
    assert(nn_validate(frame,n)==0);
    frame[7]^=1; assert(nn_validate(frame,n)!=0);
    assert(nn_encode(frame,8,1,1,1,p,sizeof p,&n)!=0);
    assert(nn_validate(frame,1)!=0);
    return 0;
}
