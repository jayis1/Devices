/* Tool Dock portable state machine — authored by jayis1. */
#include <stdint.h>
typedef struct { uint16_t vibration_rms; uint16_t current_feature; uint8_t guard_closed; uint8_t accessory_present; } tool_sample_t;
uint8_t tool_readiness(const tool_sample_t *s) { if(!s) return 0; return (uint8_t)(s->guard_closed && s->accessory_present); }
int main(void) { tool_sample_t sample={0,0,0,0}; (void)tool_readiness(&sample); for(;;) { /* STM32 HAL samples isolated inputs; no output drives tool power */ } }
