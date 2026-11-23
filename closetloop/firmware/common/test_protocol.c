#include <assert.h>
#include <string.h>
#include "protocol.h"
int main(void) { cl_event_t e={.version=1,.node_id="node-a",.seq=1,.event_id="00000000-0000-0000-0000-000000000001",.kind="worn",.item_id="shirt-1"}; assert(cl_event_valid(&e)); e.version=2; assert(!cl_event_valid(&e)); e.version=1; strcpy(e.kind,"unknown"); assert(!cl_event_valid(&e)); return 0; }
