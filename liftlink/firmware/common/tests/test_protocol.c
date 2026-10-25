#include <assert.h>
#include <stdint.h>
#include "protocol.h"
int main(void){uint8_t frame[LL_FRAME_LEN];ll_event_t a={LL_VERSION,42,77,LL_LIFT_SAMPLE,LL_LIKELY_UNAVAILABLE,90},b={0};uint32_t t=0;assert(ll_encode(&a,1234,frame)==0);assert(ll_decode(frame,sizeof frame,&b,&t)==0);assert(b.node_id==42&&b.sequence==77&&b.type==LL_LIFT_SAMPLE&&b.status==LL_LIKELY_UNAVAILABLE&&b.confidence==90&&t==1234);frame[3]^=1;assert(ll_decode(frame,sizeof frame,&b,&t)<0);frame[0]=2;assert(ll_decode(frame,sizeof frame,&b,&t)<0);assert(ll_crc16((const uint8_t*)"123456789",9)==0x29b1);return 0;}
