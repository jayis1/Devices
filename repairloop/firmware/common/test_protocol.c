#include "protocol.h"
#include <assert.h>
#include <string.h>
int main(void) {
    rl_message a = { .type = 2, .sequence = 0x12345678u, .length = 3, .payload = {1,2,3} }, b;
    uint8_t bytes[16]; size_t n = 0;
    assert(rl_encode(&a, bytes, sizeof bytes, &n) == 0 && n == 10);
    assert(rl_decode(bytes, n, &b) == 0 && b.sequence == a.sequence && b.length == 3 && memcmp(a.payload,b.payload,3)==0);
    assert(rl_decode(bytes, n-1, &b) != 0);
    assert(rl_encode(&a, bytes, 2, &n) != 0);
    bytes[0] = 9; assert(rl_decode(bytes, 10, &b) != 0);
    return 0;
}
