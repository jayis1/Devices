#include "protocol.h"
#include <assert.h>
int main(void) {
    assert(cc_validate((cc_event){1,1,CC_TASK_DONE}));
    assert(!cc_validate((cc_event){1,2,CC_TASK_DONE}));
    assert(cc_validate((cc_event){2,100000,CC_SUPPLY_G}));
    assert(!cc_validate((cc_event){2,100001,CC_SUPPLY_G}));
    assert(cc_validate((cc_event){3,2,CC_AVAILABILITY}));
    assert(!cc_validate((cc_event){3,-1,CC_AVAILABILITY}));
    assert(!cc_validate((cc_event){4,0,(cc_kind)99}));
    return 0;
}
