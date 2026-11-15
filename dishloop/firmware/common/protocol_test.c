#include "protocol.h"
#include <assert.h>
int main(void) {
 dishloop_event e = {1, 2, 24.5};
 assert(dishloop_valid(&e, 1));
 assert(!dishloop_valid(&e, 2));
 e.version = 2; assert(!dishloop_valid(&e, 0));
 e.version = 1; e.value = __builtin_nan(""); assert(!dishloop_valid(&e, 0));
 return 0;
}
