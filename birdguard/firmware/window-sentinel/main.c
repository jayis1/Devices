#include <stdint.h>
/* Hardware-specific ESP-IDF sensor and BLE drivers intentionally not included. */
struct window_sample { uint32_t sequence; uint32_t lux; uint8_t panel_present; };
int window_sample_valid(const struct window_sample *s) { return s && s->lux <= 200000u && s->panel_present <= 1u; }
