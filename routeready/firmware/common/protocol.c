#include "protocol.h"
#include <string.h>
int rr_encode(const rr_packet *p,uint8_t *o,size_t cap,size_t *n){ if(!p||!o||!n||p->length>RR_MAX_PAYLOAD)return -1; size_t z=7u+p->length;if(cap<z)return -2;o[0]=RR_VERSION;o[1]=p->kind;o[2]=p->length;o[3]=(uint8_t)(p->seq>>24);o[4]=(uint8_t)(p->seq>>16);o[5]=(uint8_t)(p->seq>>8);o[6]=(uint8_t)p->seq;memcpy(o+7,p->payload,p->length);*n=z;return 0;}
int rr_decode(const uint8_t *i,size_t n,rr_packet *p){ if(!i||!p||n<7u||i[0]!=RR_VERSION||i[2]>RR_MAX_PAYLOAD||n!=7u+i[2])return -1;p->kind=i[1];p->length=i[2];p->seq=((uint32_t)i[3]<<24)|((uint32_t)i[4]<<16)|((uint32_t)i[5]<<8)|i[6];memcpy(p->payload,i+7,p->length);return 0;}
