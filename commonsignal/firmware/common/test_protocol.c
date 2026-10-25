#include <assert.h>
#include <string.h>
#include "protocol.h"
int main(void){uint8_t b[CS_FRAME_MAX],t,l;uint16_t id,s;uint32_t u;const uint8_t*p;const uint8_t msg[]={1,2,3};size_t n=cs_encode(b,sizeof b,CS_ROUTE,42,7,99,msg,3);assert(n==18);assert(cs_decode(b,n,&t,&id,&s,&u,&p,&l));assert(t==CS_ROUTE&&id==42&&s==7&&u==99&&l==3&&!memcmp(msg,p,3));b[13]^=1;assert(!cs_decode(b,n,&t,&id,&s,&u,&p,&l));return 0;}
