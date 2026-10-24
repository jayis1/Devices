#include <stdint.h>
/* Portable policy example; one-wire driver, buzzer timer and BLE SDK are not included. */
int fridge_temperature_valid(int16_t centi_c) { return centi_c >= -4000 && centi_c <= 8500; }
int fridge_alarm_required(int16_t centi_c, uint8_t probe_ok) {
    if (!probe_ok || !fridge_temperature_valid(centi_c)) return 1; /* UNKNOWN is alarm-worthy */
    return centi_c > 500; /* configurable advisory threshold; not a food-safety decision */
}
