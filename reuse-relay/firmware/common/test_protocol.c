#include <assert.h>
#include <stdint.h>
#include "protocol.h"
int main(void){uint8_t b[RR_FRAME_SIZE];rr_message_t a={RR_WEIGHT_SAMPLE,0x12345678u,42u,-1234,3},z={0};assert(rr_encode(&a,b)==0);assert(rr_decode(b,sizeof b,&z)==0);assert(z.node_id==a.node_id&&z.sequence==a.sequence&&z.value_mg==a.value_mg&&z.flags==a.flags);b[10]^=1;assert(rr_decode(b,sizeof b,&z)==-2);assert(rr_decode(b,18,&z)==-1);return 0;}
