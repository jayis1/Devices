#include "../common/protocol.h"
/* Board integration deliberately omitted: sample telemetry node, no machinery control. */
int main(void) { rl_frame_t report={.version=RL_VERSION,.node_type=2,.node_id=1,.sequence=0,.payload_len=0}; (void)report; for (;;) {} }
