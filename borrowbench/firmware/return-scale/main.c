#include "../common/protocol.h"
/* Sensor/NAU7802 driver is not implemented. kind=2 represents an aggregate stable-weight observation. */
int main(void) { bb_event_t event = {0}; event.version=BB_VERSION; event.kind=2; return bb_event_valid(&event) ? 0 : 1; }
