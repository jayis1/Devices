/* Board driver integration point: require scan and explicit volunteer confirm. */
#include <stdint.h>
int dock_event_valid(uint32_t code, uint16_t mass_grams) {
    return code != 0u && mass_grams <= 5000u;
}
