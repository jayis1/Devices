#include <stdint.h>
/* Board adapter TODO: GPIO handlers must debounce and publish explicit state only. */
typedef enum { BENCH_AVAILABLE, BENCH_BUSY, BENCH_PAUSED, BENCH_UNKNOWN } bench_state;
static volatile bench_state state = BENCH_UNKNOWN;
void bench_on_button(uint8_t button) { if (button == 1) state = BENCH_AVAILABLE; else if (button == 2) state = BENCH_BUSY; else if (button == 3) state = BENCH_PAUSED; }
bench_state bench_state_read(void) { return state; }
