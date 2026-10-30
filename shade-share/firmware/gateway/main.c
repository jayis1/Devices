#include "../common/protocol.h"
/* Platform adapter should call this after radio receive and forward valid data. */
int gateway_validate(const uint8_t *frame,size_t len){uint32_t id;uint16_t seq,mv;uint8_t kind;int16_t value;return ss_decode(frame,len,&id,&seq,&kind,&value,&mv);}
