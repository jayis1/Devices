#include <assert.h>
#include <stdint.h>
#include "../firmware/common/protocol.h"
int main(void){pp_msg_t a={.version=PP_VERSION,.node=1,.type=1,.seq=9,.epoch_s=123,.len=2,.payload={0x34,0x12}},b;uint8_t buf[64];int n=pp_encode(&a,buf,sizeof buf);assert(n==13);assert(pp_decode(buf,n,&b)==0);assert(b.node==1&&b.seq==9&&b.payload[1]==0x12);buf[3]^=1;assert(pp_decode(buf,n,&b)==-2);return 0;}
