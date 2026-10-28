/* ESP-IDF integration point: ESP32-C3-MINI-1, TMP117 at 0x48 via SDA=GPIO6/SCL=GPIO7. */
#include "../common/protocol.h"
/* Board-specific acquisition, sleep and BLE advertising are intentionally not stubbed as working firmware. */
int probe_make_frame(int16_t centi_c, uint16_t seq, uint16_t mv, uint8_t out[MC_FRAME_SIZE]) {
 mc_frame_t f={MC_VERSION,MC_PROBE,seq,centi_c,0xff,mv}; return mc_encode(&f,out);
}
