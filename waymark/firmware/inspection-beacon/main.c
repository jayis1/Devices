#include <stdint.h>
#include <stddef.h>
size_t waymark_encode(uint8_t, uint16_t, uint16_t, const uint8_t *, size_t, uint8_t *, size_t);
/* Staff-triggered event example; GPIO/button and BLE stack integration pending. */
int main(void) {
    uint8_t confirmed=1, frame[16];
    return waymark_encode(2,2,1,&confirmed,1,frame,sizeof frame)==11 ? 0 : 1;
}
