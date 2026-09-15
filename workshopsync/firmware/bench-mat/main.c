/* Bench Mat portable state machine — authored by jayis1. */
#include <stdint.h>
typedef struct { uint16_t zone[4]; uint8_t estop_state; uint8_t calibrated; } bench_state_t;
uint8_t bench_is_clear(const bench_state_t *s) { uint8_t i; if(!s || !s->calibrated) return 0; for(i=0;i<4;i++) if(s->zone[i]>800u) return 0; return 1; }
int main(void) { bench_state_t state={{0,0,0,0},0,0}; (void)bench_is_clear(&state); for(;;) { /* RP2040 ADC sampling; e-stop state is display-only */ } }
