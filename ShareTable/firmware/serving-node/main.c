#include "../common/protocol.h"
/* ESP-IDF integration point: sample a powered DS18B20 on GPIO4, debounce GPIO5, emit bounded v1 telemetry. */
int main(void) { struct st_reading r = { ST_PROTOCOL_VERSION, 0, 0.0f }; (void)r; return 0; }
