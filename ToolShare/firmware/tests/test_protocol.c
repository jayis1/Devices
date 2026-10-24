#include <assert.h>
#include <stdint.h>
#include "../common/protocol.h"
int main(void){ts_message_t a={1,2,0x1234,77,-450,3},b={0};uint8_t f[TS_FRAME_SIZE];assert(sizeof(f)==16);assert(ts_encode(&a,f)==0);assert(ts_decode(f,sizeof f,&b)==0);assert(a.version==b.version&&a.type==b.type&&a.node_id==b.node_id&&a.sequence==b.sequence&&a.value==b.value&&a.flags==b.flags);f[8]^=1;assert(ts_decode(f,sizeof f,&b)!=0);assert(ts_decode(f,15,&b)!=0);return 0;}
