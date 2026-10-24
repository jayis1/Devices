#include <stdint.h>
/* Portable policy example; HX711 GPIO/ADC and BLE SDK integration are not included. */
int shelf_mass_record(uint32_t grams, uint32_t *last) {
    if (!last || grams > 50000u) return -1;
    if (*last == grams) return 0;
    *last = grams; /* caller reports only a change beyond calibrated deadband */
    return 1;
}
