/* Gateway policy example: accept parsed protocol records, maintain local snapshot. */
#include <stdint.h>
#include "../common/protocol.h"
int gateway_accept(const uint8_t *frame,unsigned n){uint8_t kind;uint16_t node;uint32_t seq;int32_t value;return lw_decode(frame,n,&kind,&node,&seq,&value); /* Production: replay window + persistent bounded queue. */}
