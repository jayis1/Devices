#include <stdint.h>
#include "../common/protocol.h"
/* Vendor HAL boundary; target-specific ToF/load-cell driver not yet implemented. */
int bin_sample(uint32_t fill_permille){uint8_t frame[PP_FRAME_SIZE];return fill_permille<=1000 && pp_encode(frame,2,1,PP_FILL,fill_permille,0)==PP_FRAME_SIZE ? 0 : 1;}
