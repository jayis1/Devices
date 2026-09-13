#include "protocol.h"
uint32_t ms_crc32c(const uint8_t *p,uint16_t n){uint32_t c=~0u;while(n--){c^=*p++;for(uint8_t i=0;i<8;i++)c=(c>>1)^((c&1u)?0x82F63B78u:0u);}return ~c;}
bool ms_validate(const ms_frame_t *f,uint16_t n,uint32_t last){if(n<sizeof(ms_frame_t)||f->version!=1||f->len>MS_MAX_PAYLOAD||f->seq<=last)return false;return ms_crc32c((const uint8_t*)f,(uint16_t)offsetof(ms_frame_t,crc32c))==f->crc32c;}
