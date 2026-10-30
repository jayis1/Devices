#include "protocol.h"
#include <assert.h>
#include <string.h>
int main(void){ss_message a={.node="wheel-1",.sequence=42,.uptime_ms=123456,.kind=2,.value_milli=-1250},b={0};uint8_t buf[SS_PAYLOAD_MAX];size_t n=0;assert(ss_encode(&a,buf,sizeof buf,&n)==0);assert(ss_decode(buf,n,&b)==0);assert(!strcmp(a.node,b.node)&&a.sequence==b.sequence&&a.value_milli==b.value_milli);assert(ss_decode(buf,n-1,&b)!=0);buf[0]=9;assert(ss_decode(buf,n,&b)!=0);assert(ss_encode(&a,buf,2,&n)!=0);return 0;}
