/* Board driver integration point: acquire bounded SHT31 sample, frame, BLE notify. */
#include <stdint.h>
int storage_sample_valid(int16_t centi_c, uint16_t centi_rh) {
    return centi_c >= -4000 && centi_c <= 8500 && centi_rh <= 10000;
}
