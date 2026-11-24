/* Staff button reports existing transmitter state; no audio capture/transmission. SDK integration omitted. */
#include "../common/protocol.h"
int main(void) { uint8_t frame[LL_FRAME_SIZE]; return ll_encode(frame,3,1,1,2900) ? 0 : 1; }
