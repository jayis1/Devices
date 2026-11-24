#include "protocol.h"
static uint8_t crc8(const uint8_t *p, size_t n) {
    uint8_t c=0;
    for (size_t i=0;i<n;i++) {
        c^=p[i];
        for (unsigned b=0;b<8;b++) {
            uint8_t shifted=(uint8_t)(c<<1);
            c=(c&0x80u)?(uint8_t)(shifted^0x07u):shifted;
        }
    }
    return c;
}
int ll_encode(uint8_t o[LL_FRAME_SIZE], uint16_t id, uint32_t s, uint8_t f, uint16_t mv) {
    if (!o) return 0;
    o[0]='L';o[1]='L';o[2]=LL_VERSION;o[3]=(uint8_t)(id>>8);o[4]=(uint8_t)id;
    o[5]=(uint8_t)(s>>24);o[6]=(uint8_t)(s>>16);o[7]=(uint8_t)(s>>8);o[8]=(uint8_t)s;
    o[9]=f;o[10]=(uint8_t)(mv>>8);o[11]=(uint8_t)mv;o[12]=crc8(o,12);return 1;
}
int ll_decode(const uint8_t *i,size_t n,uint16_t *id,uint32_t *s,uint8_t *f,uint16_t *mv) {
    if (!i||n!=LL_FRAME_SIZE||i[0]!='L'||i[1]!='L'||i[2]!=LL_VERSION||crc8(i,12)!=i[12]) return 0;
    if(id)*id=(uint16_t)((uint16_t)i[3]<<8|i[4]);
    if(s)*s=(uint32_t)i[5]<<24|(uint32_t)i[6]<<16|(uint32_t)i[7]<<8|i[8];
    if(f)*f=i[9];
    if(mv)*mv=(uint16_t)((uint16_t)i[10]<<8|i[11]);
    return 1;
}
