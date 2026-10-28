#include "protocol.h"
static void put32(uint8_t *p, uint32_t v) { for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(8u*i)); }
static uint32_t get32(const uint8_t *p) { uint32_t v=0; for (unsigned i=0;i<4;i++) v|=(uint32_t)p[i]<<(8u*i); return v; }
uint16_t pp_crc16(const uint8_t *data, size_t len) {
  uint16_t crc=0xffffu;
  for(size_t i=0;i<len;i++){ crc^=(uint16_t)data[i]<<8; for(unsigned b=0;b<8;b++) crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1); }
  return crc;
}
int pp_encode(uint8_t out[PP_FRAME_SIZE], uint8_t type, uint32_t node_id, uint32_t sequence, int32_t value) {
  if(!out || type<1u || type>3u) return -1;
  out[0]=PP_VERSION; out[1]=type; put32(out+2,node_id); put32(out+6,sequence); put32(out+10,(uint32_t)value);
  uint16_t c=pp_crc16(out,14); out[14]=(uint8_t)c; out[15]=(uint8_t)(c>>8); return 0;
}
int pp_decode(const uint8_t *f, size_t len, uint8_t *type, uint32_t *node_id, uint32_t *sequence, int32_t *value) {
  if(!f || len!=PP_FRAME_SIZE || !type || !node_id || !sequence || !value || f[0]!=PP_VERSION || f[1]<1u || f[1]>3u) return -1;
  uint16_t expected=(uint16_t)f[14]|((uint16_t)f[15]<<8); if(pp_crc16(f,14)!=expected) return -2;
  *type=f[1]; *node_id=get32(f+2); *sequence=get32(f+6); *value=(int32_t)get32(f+10); return 0;
}
