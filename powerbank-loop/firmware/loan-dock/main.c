#include <stdint.h>
/* Portable role-state examples only: no board SDK, radio driver, or safety-certified charger control. */
typedef struct { uint8_t occupied, ready, faults; uint32_t seq; } dock_status_t;
int dock_status_valid(const dock_status_t *s, uint8_t slots) { return s && slots && s->occupied<=slots && s->ready<=s->occupied && s->faults<=slots; }
int dock_output_permitted(int temperature_valid, int temperature_deci_c, int hardware_fault) { return temperature_valid && temperature_deci_c>=50 && temperature_deci_c<=450 && !hardware_fault; }
