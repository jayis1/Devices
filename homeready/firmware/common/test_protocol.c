#include "protocol.h"
#include <assert.h>
int main(void) {
 const uint8_t ok[] = "{\"version\":1}";
 const uint8_t bad[] = "[]";
 assert(hr_event_envelope_valid(ok, sizeof(ok)-1u));
 assert(!hr_event_envelope_valid(bad, sizeof(bad)-1u));
 assert(!hr_event_envelope_valid(ok, 0u));
 assert(!hr_event_envelope_valid(ok, HR_MAX_EVENT_BYTES+1u));
 return 0;
}
