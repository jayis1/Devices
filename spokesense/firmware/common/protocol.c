#include "protocol.h"
#include <string.h>
static void put32(uint8_t *p,uint32_t x){p[0]=x;p[1]=x>>8;p[2]=x>>16;p[3]=x>>24;}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int ss_encode(const ss_message*m,uint8_t*out,size_t cap,size_t*w){if(!m||!out||!w)return -1;size_t n=0;while(n<=SS_NODE_MAX&&m->node[n])n++;if(!n||n>SS_NODE_MAX)return -2;size_t need=15+n;if(cap<need)return -3;out[0]=1;out[1]=(uint8_t)n;memcpy(out+2,m->node,n);put32(out+2+n,m->sequence);put32(out+6+n,m->uptime_ms);out[10+n]=m->kind;put32(out+11+n,(uint32_t)m->value_milli);*w=need;return 0;}
int ss_decode(const uint8_t*in,size_t len,ss_message*out){if(!in||!out||len<16||in[0]!=1)return -1;size_t n=in[1];if(!n||n>SS_NODE_MAX||len!=15+n)return -2;memcpy(out->node,in+2,n);out->node[n]=0;out->sequence=get32(in+2+n);out->uptime_ms=get32(in+6+n);out->kind=in[10+n];out->value_milli=(int32_t)get32(in+11+n);return 0;}
