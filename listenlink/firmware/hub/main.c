/* Hub contract: scan room-beacon v1, read dock count, mark stale >120s, publish via configured TLS MQTT. SDK integration omitted. */
#include "../common/protocol.h"
int main(void) { uint8_t frame[LL_FRAME_SIZE]; return ll_encode(frame,1,1,0,3000) ? 0 : 1; }
