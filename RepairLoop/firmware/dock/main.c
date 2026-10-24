#include "../common/protocol.h"
/* Illustrative dock telemetry entry point; bind to nRF Connect SDK before hardware use. */
int main(void) { rl_frame_t report={.version=RL_VERSION,.node_type=3,.node_id=2,.sequence=0,.payload_len=0}; (void)report; for (;;) {} }
