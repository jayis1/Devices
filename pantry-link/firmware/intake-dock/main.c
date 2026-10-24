#include <stdint.h>
/* Manual confirmation is required: the scale cannot infer item identity. */
int intake_record(uint16_t item_class, uint32_t grams) {
    return (item_class != 0u && grams > 0u && grams <= 30000u) ? 0 : -1;
}
