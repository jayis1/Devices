#include <stdint.h>
/* Firmware integration point: local staff button drives LED feedback; no cabinet actuation. */
uint8_t inspection_beacon_led_pattern(uint8_t event_accepted) {
    return event_accepted ? 1u : 2u; /* steady confirmation / double-flash error */
}
