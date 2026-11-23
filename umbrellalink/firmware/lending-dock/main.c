#include <stdint.h>
/* ESP-IDF integration TODO: sample four active-low switches with debounce,
 * form protocol v1 packet and send through the RFM69 driver. */
int lending_free_slots(const uint8_t occupied[4]) {
    int free_count = 0;
    for (int i=0;i<4;i++) free_count += occupied[i] ? 0 : 1;
    return free_count;
}
