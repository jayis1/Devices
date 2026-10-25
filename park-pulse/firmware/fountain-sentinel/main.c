#include <stdint.h>
#include "../common/protocol.h"
/* Vendor HAL boundary; target-specific GPIO/radio driver not yet implemented. */
int fountain_sample(uint32_t pulse_count,uint32_t leak){uint8_t frame[PP_FRAME_SIZE];return pp_encode(frame,1,1,PP_FLOW,pulse_count,0)==PP_FRAME_SIZE && pp_encode(frame,1,2,PP_LEAK,leak,0)==PP_FRAME_SIZE ? 0 : 1;}
