/* Sample 8 MCP23017 bay switches, debounce 30ms; board driver integration omitted. */
#include "../common/protocol.h"
int main(void) { uint8_t frame[LL_FRAME_SIZE]; return ll_encode(frame,2,1,0,5000) ? 0 : 1; }
