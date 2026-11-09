#include "protocol.h"
#include <assert.h>
#include <string.h>
int main(void) {
    mm_packet_t in={.node=2,.seq=0x12345678u,.kind=1,.length=3,.payload={0,127,255}}, out={0};
    uint8_t bytes[42]; size_t n=mm_encode(&in,bytes,sizeof bytes);
    assert(n==13 && mm_decode(bytes,n,&out));
    assert(out.node==in.node && out.seq==in.seq && out.kind==in.kind && out.length==in.length);
    assert(memcmp(out.payload,in.payload,in.length)==0);
    assert(!mm_decode(bytes,n-1,&out)); bytes[0]=9; assert(!mm_decode(bytes,n,&out));
    bytes[0]=MM_VERSION; bytes[8]^=1; assert(!mm_decode(bytes,n,&out));
    in.length=33; assert(mm_encode(&in,bytes,sizeof bytes)==0);
    return 0;
}
