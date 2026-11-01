#include <math.h>
#include <assert.h>
#include <string.h>
#include "protocol.h"
int main(void) {
 ps_message m;
 const char *good="{\"v\":1,\"id\":\"rack-01\",\"node\":\"rack\",\"seq\":1,\"ts_ms\":1234,\"kind\":\"stock\",\"value\":12,\"unit\":\"items\"}";
 assert(ps_validate(good,&m)); assert(m.seq==1 && m.value==12);
 assert(!ps_validate("{}",&m));
 assert(!ps_validate("{\"v\":2,\"id\":\"x\",\"node\":\"rack\",\"seq\":1,\"ts_ms\":1,\"kind\":\"stock\",\"value\":1,\"unit\":\"items\"}",&m));
}
