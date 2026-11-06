#include <stdint.h>
/* Outdoor node samples ambient lux, temp/RH and rain contact; no actuation. */
int outdoor_lux_valid(uint32_t lux) { return lux <= 120000; }
