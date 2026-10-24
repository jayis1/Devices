#include "protocol.h"
#include <assert.h>
int main(void) {
    const uint8_t check[]="123456789";
    assert(ll_crc16(check,9u)==0x29b1u);
    ll_telemetry_t in={.kind=1u,.node_id=7u,.sequence=65530u,.occupied_mask=5u,.valid_mask=15u,.uptime_s=123456u},out={0};
    uint8_t frame[LL_FRAME_SIZE];
    assert(ll_encode(frame,&in)==0); assert(ll_decode(&out,frame,sizeof frame)==0);
    assert(in.kind==out.kind && in.node_id==out.node_id && in.sequence==out.sequence && in.occupied_mask==out.occupied_mask && in.valid_mask==out.valid_mask && in.uptime_s==out.uptime_s);
    frame[7]^=1u; assert(ll_decode(&out,frame,sizeof frame)!=0);
    assert(ll_decode(&out,frame,19u)!=0);
    return 0;
}
