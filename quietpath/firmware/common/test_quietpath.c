#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "quietpath.h"
int main(void) {
    qp_report_t a={42,65535,29700000,6342,9123,1}, b;
    uint8_t f[QP_FRAME_LEN];
    assert(qp_encode(&a,f)==0); assert(qp_decode(f,sizeof f,&b)==0);
    assert(b.node_id==a.node_id && b.sequence==a.sequence && b.leq_cdb==a.leq_cdb && b.flags==a.flags);
    assert(qp_decode(f,sizeof f-1,&b)<0); f[8]^=1; assert(qp_decode(f,sizeof f,&b)<0);
    puts("protocol round-trip and corruption checks passed"); return 0;
}
