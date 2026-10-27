#include <assert.h>
#include <string.h>
#include "../firmware/common/protocol.h"
int main(void) {
    cupcycle_frame_t a={1,2,1,65530,1700000000u,42}, b={0};
    uint8_t bytes[CUPCYCLE_FRAME_LEN];
    assert(cupcycle_encode(&a,bytes)==0);
    assert(cupcycle_decode(bytes,sizeof bytes,&b)==0);
    assert(a.version==b.version && a.node==b.node && a.kind==b.kind && a.sequence==b.sequence && a.unix_s==b.unix_s && a.value==b.value);
    bytes[9]^=1; assert(cupcycle_decode(bytes,sizeof bytes,&b)==-2);
    assert(cupcycle_decode(bytes,12,&b)==-1);
    return 0;
}
