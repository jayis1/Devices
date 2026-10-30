#include "protocol.h"
#include <assert.h>
int main(void){const uint8_t ok[]="{}"; const uint8_t bad[]="[]"; assert(bd_payload_valid(ok,2)); assert(!bd_payload_valid(bad,2)); assert(!bd_payload_valid(ok,161)); return 0;}
