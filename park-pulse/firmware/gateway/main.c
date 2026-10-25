#include <stdint.h>
#include "../common/protocol.h"
/* Pi gateway integration belongs in software/dashboard; this host shim validates frames. */
int gateway_validate(const uint8_t *frame,unsigned length){return pp_decode(frame,length,0,0,0,0,0);}
