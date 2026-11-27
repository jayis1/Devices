#include "../common/event.h"

/* ESP-IDF GPIO bring-up reference. GPIO9 button (active low), GPIO8 LED. */
void app_main(void) {
    volatile uint32_t protocol_version = PARKKEYS_PROTOCOL_VERSION;
    (void)protocol_version;
    /* TODO in prototype: configure GPIO using ESP-IDF; no network is implemented. */
}
