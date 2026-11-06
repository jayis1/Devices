#include "../common/protocol.h"
/* ESP-IDF gateway integration point: BLE receive, bounded offline queue, MQTT/TLS publish. */
int main(void) { return ST_PROTOCOL_VERSION == 1 ? 0 : 1; }
