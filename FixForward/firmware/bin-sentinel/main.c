#include "../common/protocol.h"
/* Portable node logic example; target HAL and radio driver intentionally required. */
int bin_sentinel_make_telemetry(uint32_t id,uint32_t seq,int32_t value,ff_message*out){if(!out||!id)return -1;*out=(ff_message){FF_VERSION,1,id,seq,value,1};return ff_valid(out)?0:-2;}
