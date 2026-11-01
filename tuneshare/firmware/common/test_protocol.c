#include "protocol.h"
#include <assert.h>
int main(void) {
    const uint8_t ok[] = "{\"v\":1}";
    const uint8_t bad[] = "[]";
    assert(ts_payload_valid(ok, sizeof ok - 1u));
    assert(!ts_payload_valid(bad, sizeof bad - 1u));
    assert(!ts_payload_valid(ok, TS_MAX_PAYLOAD + 1u));
    assert(!ts_payload_valid(NULL, 1u));
    return 0;
}
