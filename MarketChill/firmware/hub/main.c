/* Portable alarm policy core; ESP-IDF BLE/Wi-Fi/storage integration is not implemented. */
#include <stdbool.h>
#include <stdint.h>
/* Unknown/stale must be surfaced as a fault by the board integration. */
bool hub_temperature_alarm(bool valid, bool fresh, int16_t centi_c, int16_t configured_limit) {
    if (!valid || !fresh) return true;
    return centi_c > configured_limit;
}
