/* WorkshopSync protocol reference — authored by jayis1. */
#include "workshopsync_protocol.h"
uint32_t ws_crc32c(const uint8_t *data, size_t len) { uint32_t crc=0xffffffffu; size_t i; unsigned bit; for(i=0;i<len;i++){crc^=data[i]; for(bit=0;bit<8;bit++) crc=(crc>>1)^((crc&1u)?0x82f63b78u:0u);} return ~crc; }
bool ws_frame_valid(const ws_frame_t *f,size_t wire_len,uint32_t last_seq,uint32_t expected_epoch) { size_t header=15u; size_t expected; if(!f || f->version!=WS_PROTOCOL_VERSION || f->length>WS_MAX_PAYLOAD) return false; expected=header+(size_t)f->length+sizeof(f->crc32c); if(wire_len!=expected || f->epoch!=expected_epoch || f->seq<=last_seq) return false; return ws_crc32c((const uint8_t*)f,header+f->length)==f->crc32c; }
