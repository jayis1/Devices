#include "protocol.h"
#include <assert.h>
int main(void){rb_packet_t a={.type=2,.node_id=3,.boot_id=0x12345678,.sequence=42,.uptime_s=99,.payload_len=3,.payload={1,2,3}},b={0};uint8_t buf[RB_MAX_FRAME];size_t n=rb_encode(&a,buf,sizeof buf);assert(n==21);assert(rb_decode(buf,n,&b));assert(b.node_id==3&&b.sequence==42&&b.payload[2]==3);buf[4]^=1;assert(!rb_decode(buf,n,&b));assert(!rb_decode(buf,n-1,&b));return 0;}
