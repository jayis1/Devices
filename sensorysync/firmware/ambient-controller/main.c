/* STM32G0B1 reference: PA8 LED PWM, PB0 SELV fan enable, authored by jayis1. */
#include <stdint.h>
#include "../common/sensorysync_protocol.h"
static float bounded_level(float v) { return v<0.0f?0.0f:(v>1.0f?1.0f:v); }
int main(void) { float output=bounded_level(0.5f); ss_frame_t f={0}; f.version=1; f.node_id=48; return (output>=0.0f && ss_validate(&f,0,0)) ? 0 : 1; }
