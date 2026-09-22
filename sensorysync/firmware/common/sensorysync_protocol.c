/* Authored by jayis1. */
#include "sensorysync_protocol.h"
uint32_t ss_crc32c(const uint8_t *data, size_t length) { uint32_t crc=0xffffffffu; size_t i; for(i=0;i<length;i++){crc^=data[i]; for(unsigned b=0;b<8;b++) crc=(crc>>1)^((crc&1u)?0x82f63b78u:0u);} return ~crc; }
bool ss_validate(const ss_frame_t *f, uint32_t last_seq, uint32_t now_epoch) { if(!f || f->version != SS_PROTOCOL_VERSION || f->payload_len > SS_MAX_PAYLOAD) return false; if(f->seq <= last_seq || f->epoch > now_epoch + 30u) return false; return true; }
