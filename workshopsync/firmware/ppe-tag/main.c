/* PPE Tag portable state machine — authored by jayis1. */
#include <stdint.h>
typedef struct { uint8_t paired; uint8_t worn; uint8_t acknowledged; uint16_t battery_mv; } ppe_state_t;
uint8_t ppe_ready(const ppe_state_t *s) { return s && s->paired && s->worn && s->battery_mv>=2700u; }
int main(void) { ppe_state_t state={0,0,0,3000}; (void)ppe_ready(&state); for(;;) { /* Zephyr/nRF BLE advertising and button debounce */ } }
