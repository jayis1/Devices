#include "../common/protocol.h"
/* ESP-IDF integration placeholder: publish kind=1 when local tag presence changes. */
int main(void) { bb_event_t event = {0}; event.version=BB_VERSION; event.kind=1; return bb_event_valid(&event) ? 0 : 1; }
