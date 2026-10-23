/* jayis1 — explicit little-endian serialization; CRC is not authentication. */
#include "protocol.h"
static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
uint16_t hr_crc16(const uint8_t *data, size_t length) {
    uint16_t crc=0xffffu;
    for(size_t i=0;i<length;i++){ crc^=(uint16_t)data[i]<<8; for(unsigned b=0;b<8;b++) crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1); }
    return crc;
}
int hr_encode(uint8_t *out,size_t capacity,uint8_t type,uint32_t node_id,uint32_t sequence,const uint8_t *payload,uint16_t payload_len,size_t *written) {
    size_t total=HR_HEADER_SIZE+(size_t)payload_len+HR_CRC_SIZE;
    if(!out||!written||payload_len>HR_MAX_PAYLOAD||capacity<total||(payload_len&& !payload)) return -1;
    put16(out,0x4852u); out[2]=1; out[3]=type; put32(out+4,node_id); put32(out+8,sequence); put16(out+12,payload_len);
    for(uint16_t i=0;i<payload_len;i++) out[HR_HEADER_SIZE+i]=payload[i];
    put16(out+HR_HEADER_SIZE+payload_len,hr_crc16(out,HR_HEADER_SIZE+payload_len)); *written=total; return 0;
}
int hr_validate(const uint8_t *frame,size_t length) {
    if(!frame||length<HR_HEADER_SIZE+HR_CRC_SIZE||frame[0]!=0x52||frame[1]!=0x48||frame[2]!=1) return -1;
    uint16_t n=(uint16_t)(frame[12]|((uint16_t)frame[13]<<8));
    if(n>HR_MAX_PAYLOAD||length!=HR_HEADER_SIZE+(size_t)n+HR_CRC_SIZE) return -1;
    uint16_t got=(uint16_t)(frame[HR_HEADER_SIZE+n]|((uint16_t)frame[HR_HEADER_SIZE+n+1]<<8));
    return got==hr_crc16(frame,HR_HEADER_SIZE+n)?0:-1;
}
