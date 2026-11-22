#include <assert.h>
#include <stddef.h>
#include <string.h>
int cp_event_valid(const char *, size_t);
int main(void) {
    char ok[] = "{\"v\":1,\"node\":\"evse-01\",\"seq\":42,\"ts\":1730000000,\"kind\":\"session\"}";
    char bad[] = "{\"v\":2,\"node\":\"evse-01\",\"seq\":42,\"ts\":1,\"kind\":\"session\"}";
    assert(cp_event_valid(ok, strlen(ok)));
    assert(!cp_event_valid(bad, strlen(bad)));
    assert(!cp_event_valid("{}", 2));
    assert(!cp_event_valid(ok, 513));
    return 0;
}
