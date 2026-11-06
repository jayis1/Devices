#include <stdint.h>
/* Hardware-specific ESP-IDF sensor and BLE drivers intentionally not included. */
struct weather_sample { uint32_t sequence; int16_t temperature_centi_c; uint16_t humidity_centi_pct; uint32_t pressure_centi_hpa; };
int weather_sample_valid(const struct weather_sample *s) { return s && s->temperature_centi_c >= -4000 && s->temperature_centi_c <= 8500 && s->humidity_centi_pct <= 10000 && s->pressure_centi_hpa >= 30000 && s->pressure_centi_hpa <= 120000; }
