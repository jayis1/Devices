#include "protocol.h"
uint16_t ll_crc16(const uint8_t *data, size_t len) {
 uint16_t crc=0xffffu; size_t i; unsigned b;
 for(i=0;i<len;i++){ crc^=(uint16_t)data[i]<<8; for(b=0;b<8;b++) crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1); } return crc;
}
static void put32(uint8_t*p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static uint32_t get32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int ll_encode(const ll_event_t *e,uint32_t uptime,uint8_t out[LL_FRAME_LEN]){
 uint16_t crc; if(!e||!out||e->version!=LL_VERSION||e->type<LL_HEARTBEAT||e->type>LL_STAFF_ACK||e->status>LL_STAFF_AVAILABLE||e->confidence>100) return -1;
 out[0]=e->version;out[1]=(uint8_t)e->node_id;out[2]=(uint8_t)(e->node_id>>8);put32(out+3,e->sequence);out[7]=e->type;put32(out+8,uptime);out[12]=e->status;out[13]=e->confidence;
 crc=ll_crc16(out,14);out[14]=(uint8_t)crc;out[15]=(uint8_t)(crc>>8);return 0;
}
int ll_decode(const uint8_t *in,size_t len,ll_event_t *e,uint32_t *uptime){
 if(!in||!e||!uptime||len!=LL_FRAME_LEN||in[0]!=LL_VERSION||in[7]<LL_HEARTBEAT||in[7]>LL_STAFF_ACK||in[12]>LL_STAFF_AVAILABLE||in[13]>100||((uint16_t)in[14]|((uint16_t)in[15]<<8))!=ll_crc16(in,14)) return -1;
 e->version=in[0];e->node_id=(uint16_t)in[1]|((uint16_t)in[2]<<8);e->sequence=get32(in+3);e->type=in[7];*uptime=get32(in+8);e->status=in[12];e->confidence=in[13];return 0;
}
