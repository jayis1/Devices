#include "../common/protocol.h"
#include <assert.h>
#include <string.h>

int main(void) {
    rw_rack_report_t a = { .seq = 0x12345678u, .count = 3, .bay = {RW_OCCUPIED, RW_CLEAR, RW_UNKNOWN} }, b = {0};
    uint8_t wire[14];
    size_t n = rw_encode(&a, wire, sizeof wire);
    assert(n == 9 && rw_decode(wire, n, &b));
    assert(b.seq == a.seq && b.count == a.count && memcmp(a.bay, b.bay, 3) == 0);
    assert(!rw_decode(wire, n - 1, &b));
    assert(!rw_decode(wire, n + 1, &b));
    wire[0] = 9; assert(!rw_decode(wire, n, &b));
    wire[0] = RW_VERSION; wire[6] = 255; assert(!rw_decode(wire, n, &b));
    assert(rw_encode(&a, wire, 1) == 0);
    return 0;
}
