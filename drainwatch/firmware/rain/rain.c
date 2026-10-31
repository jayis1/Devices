#include "../common/protocol.h"
/* Rain gauge debounce/counting belongs in target GPIO adapter. */
int rain_make_report(uint32_t node,uint32_t seq,int32_t interval_tips,uint8_t frame[DW_FRAME_SIZE]) { if(interval_tips<0||interval_tips>10000) return -1; return dw_encode(frame,DW_KIND_RAIN,node,seq,interval_tips); }
