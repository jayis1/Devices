#include <assert.h>
#include <string.h>
#include "protocol.h"
int main(void){pp_frame_t a={PP_VERSION,2,0x12345678u,42,900,321,1},b={0};uint8_t buf[PP_FRAME_LEN];assert(pp_encode(&a,buf)==0);assert(pp_decode(buf,sizeof buf,&b)==0);assert(a.version==b.version&&a.node_type==b.node_type&&a.node_id==b.node_id&&a.sequence==b.sequence&&a.age_ms==b.age_ms&&a.value==b.value&&a.status==b.status);buf[4]^=1;assert(pp_decode(buf,sizeof buf,&b)<0);return 0;}
