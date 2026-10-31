#include "../common/protocol.h"
/* Platform-neutral acquisition boundary. Board SDK sensor/radio integration is intentionally not included. */
int inlet_make_report(uint32_t node, uint32_t seq, int32_t distance_mm, int valid, uint8_t frame[DW_FRAME_SIZE]) { if(!valid || distance_mm<0 || distance_mm>4000) return -1; return dw_encode(frame,DW_KIND_INLET,node,seq,distance_mm); }
