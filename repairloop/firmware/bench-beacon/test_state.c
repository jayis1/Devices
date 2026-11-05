#include <assert.h>
#include "main.c"
int main(void) { assert(bench_state_read()==BENCH_UNKNOWN); bench_on_button(1); assert(bench_state_read()==BENCH_AVAILABLE); bench_on_button(9); assert(bench_state_read()==BENCH_AVAILABLE); return 0; }
