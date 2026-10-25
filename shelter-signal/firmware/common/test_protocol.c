#include "protocol.h"
#include <assert.h>
int main(void){ss_message_t a={SS_VERSION,SS_APPROACH,42,123,30,-1},b={0};uint8_t f[SS_FRAME_SIZE];assert(ss_encode(&a,f)==0);assert(ss_decode(f,sizeof f,&b)==0);assert(b.node_id==42&&b.sequence==123&&b.value==-1&&b.kind==SS_APPROACH);f[5]^=1;assert(ss_decode(f,sizeof f,&b)==-2);assert(ss_decode(f,19,&b)==-1);assert(ss_crc16((const uint8_t*)"123456789",9)==0x29b1);return 0;}
