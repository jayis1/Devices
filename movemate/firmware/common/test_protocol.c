#include "protocol.h"
#include <assert.h>
int main(void){mm_message a={.version=1,.type=2,.length=3,.sequence=0x12345678,.payload={1,2,3}},b={0};uint8_t buf[140];size_t n=0;assert(mm_encode(&a,buf,sizeof buf,&n)==0);assert(mm_decode(buf,n,&b)==0);assert(b.sequence==a.sequence&&b.length==3&&b.payload[2]==3);buf[n-1]^=1;assert(mm_decode(buf,n,&b)==-3);assert(mm_decode(buf,n-1,&b)<0);return 0;}
