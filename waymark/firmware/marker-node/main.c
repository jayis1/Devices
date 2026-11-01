#include <stdint.h>
#include <stddef.h>
/* Board integration deliberately separated: call this encoder with a sampled
 * marker state; BLE advertising and sensor drivers remain unimplemented. */
size_t waymark_encode(uint8_t, uint16_t, uint16_t, const uint8_t *, size_t, uint8_t *, size_t);
int main(void) {
    uint8_t present=1, frame[16];
    size_t n=waymark_encode(1,1,1,&present,1,frame,sizeof frame);
    return n == 11 ? 0 : 1;
}
