#include "../common/protocol.h"
/* Local button confirmation producer; there is deliberately no remote actuation path. */
int main(void) { bb_event_t event = {0}; event.version=BB_VERSION; event.kind=3; return bb_event_valid(&event) ? 0 : 1; }
