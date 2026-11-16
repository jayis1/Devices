#include <assert.h>
#include <string.h>
#include "../common/protocol.h"
int main(void) {
 rt_event e={.version=1,.kind=1,.seq=4,.timestamp=123}; strcpy(e.node,"dock-01"); strcpy(e.book,"bk-123"); assert(rt_valid(&e));
 e.version=2; assert(!rt_valid(&e)); e.version=1; e.kind=9; assert(!rt_valid(&e)); e.kind=1; e.node[0]=0; assert(!rt_valid(&e));
 memset(e.node,'x',sizeof e.node); assert(!rt_valid(&e)); return 0;
}
