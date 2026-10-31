#include <assert.h>
#include "protocol.h"
int main(void) {
    const shiftshare_frame_t good = {1, 1, 1u, 42u};
    const shiftshare_frame_t bad = {2, 1, 1u, 42u};
    assert(shiftshare_frame_valid(&good));
    assert(!shiftshare_frame_valid(&bad));
    assert(!shiftshare_frame_valid(NULL));
    return 0;
}
