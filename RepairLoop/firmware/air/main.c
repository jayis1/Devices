#include "../common/protocol.h"
/* Illustrative read-only ventilation telemetry node. */
int main(void) { rl_frame_t report={.version=RL_VERSION,.node_type=4,.node_id=3,.sequence=0,.payload_len=0}; (void)report; for (;;) {} }
