/* RP2040 reference: GPIO10/11/12 I2S, GPIO15 haptic, authored by jayis1. */
#include <stdint.h>
#include "../common/sensorysync_protocol.h"
static uint8_t outputs_enabled;
static void local_stop(void) { outputs_enabled=0u; }
int main(void) { ss_frame_t f={0}; f.version=1; f.node_id=64; local_stop(); return (outputs_enabled==0u && ss_validate(&f,0,0)) ? 0 : 1; }
