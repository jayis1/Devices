#include "protocol.h"
uint16_t rl_crc16(const uint8_t *p, size_t n) {
    uint16_t crc = 0xffffu;
    for (size_t i=0; i<n; ++i) {
        crc ^= (uint16_t)((uint16_t)p[i] << 8);
        for (unsigned b=0;b<8;++b) {
            uint16_t shifted = (uint16_t)(crc << 1);
            crc = (crc & 0x8000u) ? (uint16_t)(shifted ^ 0x1021u) : shifted;
        }
    }
    return crc;
}
int rl_encode(const rl_frame_t *f, uint8_t *o, size_t cap, size_t *w) {
    if (!f || !o || !w || f->version != RL_VERSION || f->payload_len > RL_MAX_PAYLOAD) return -1;
    size_t n=RL_HEADER_SIZE+f->payload_len+RL_CRC_SIZE; if(cap<n) return -2;
    o[0]=f->version; o[1]=f->node_type; o[2]=(uint8_t)f->node_id; o[3]=(uint8_t)(f->node_id>>8); o[4]=(uint8_t)f->sequence; o[5]=(uint8_t)(f->sequence>>8); o[6]=f->payload_len;
    for(size_t i=0;i<f->payload_len;++i) o[7+i]=f->payload[i];
    uint16_t c=rl_crc16(o,n-2); o[n-2]=(uint8_t)c; o[n-1]=(uint8_t)(c>>8); *w=n; return 0;
}
int rl_decode(const uint8_t *d, size_t n, rl_frame_t *f) {
    if(!d||!f||n<RL_HEADER_SIZE+RL_CRC_SIZE||n>RL_MAX_FRAME||d[0]!=RL_VERSION||d[6]>RL_MAX_PAYLOAD||n!=(size_t)RL_HEADER_SIZE+d[6]+RL_CRC_SIZE)return -1;
    uint16_t got=(uint16_t)d[n-2]|((uint16_t)d[n-1]<<8); if(rl_crc16(d,n-2)!=got)return -2;
    f->version=d[0];f->node_type=d[1];f->node_id=(uint16_t)d[2]|((uint16_t)d[3]<<8);f->sequence=(uint16_t)d[4]|((uint16_t)d[5]<<8);f->payload_len=d[6];for(size_t i=0;i<f->payload_len;++i)f->payload[i]=d[7+i];return 0;
}
