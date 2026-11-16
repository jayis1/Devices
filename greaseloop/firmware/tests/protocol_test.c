#include "../common/protocol.h"
#include <assert.h>
int main(void) {
 const char *ok="{\"v\":1,\"node\":\"dock-01\",\"seq\":1,\"ts\":1730000000,\"kind\":\"deposit\",\"mass_g\":230}";
 assert(gl_valid_event(ok, strlen(ok)));
 assert(!gl_valid_event(ok, 513));
 assert(!gl_valid_event("{}", 2));
 assert(!gl_valid_event(0, 1));
 return 0;
}
