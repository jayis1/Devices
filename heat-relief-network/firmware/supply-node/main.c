/* jayis1 — portable node-loop demonstration; replace callbacks with board HAL. */
#include <stdint.h>
static int sample_load_and_beam(void) { return 0; }
static int send_telemetry(void) { return 0; }
int main(void) { for (;;) { if(sample_load_and_beam()!=0) continue; (void)send_telemetry(); } }
