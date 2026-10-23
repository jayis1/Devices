/* jayis1 — portable node-loop demonstration; replace callbacks with board HAL. */
#include <stdint.h>
static int sample_sensors(void) { return 0; }
static int send_telemetry(void) { return 0; }
int main(void) { for (;;) { if(sample_sensors()!=0) continue; (void)send_telemetry(); } }
