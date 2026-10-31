#include "../common/protocol.h"
/* Gateway validation only; radio receive loop and persistent storage require target SDK integration. */
int gateway_accept(const uint8_t *frame,size_t len,uint8_t *kind,uint32_t *node,uint32_t *seq,int32_t *value) { return dw_decode(frame,len,kind,node,seq,value); }
