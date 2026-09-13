#include "protocol.h"
uint32_t hs_crc32c(const uint8_t *p, uint16_t n) { uint32_t c=~0u; while(n--) { c^=*p++; for(uint8_t b=0;b<8;b++) c=(c>>1)^((c&1u)?0x82F63B78u:0u); } return ~c; }
bool hs_validate(const hs_frame_t *f,uint16_t n,uint32_t last) { if(!f || f->version!=1 || f->len>HS_MAX_PAYLOAD || n!=(uint16_t)(18+f->len+HS_TAG_LEN) || f->seq<=last) return false; return hs_crc32c((const uint8_t*)f,14+f->len)==f->crc32c; }
