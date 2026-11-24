#include "protocol.h"
#include <assert.h>
#include <string.h>
int main(void){rr_packet a={.kind=2,.seq=0x12345678,.length=2,.payload={9,8}},b;uint8_t wire[128];size_t n=0;assert(rr_encode(&a,wire,sizeof wire,&n)==0);assert(rr_decode(wire,n,&b)==0);assert(b.seq==a.seq&&b.kind==a.kind&&b.length==2&&memcmp(a.payload,b.payload,2)==0);assert(rr_decode(wire,n-1,&b)!=0);wire[0]=99;assert(rr_decode(wire,n,&b)!=0);return 0;}
