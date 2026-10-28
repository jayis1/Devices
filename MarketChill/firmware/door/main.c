/* ESP-IDF integration point: ESP32-C3-MINI-1; normally-closed contact GPIO4 to GND. */
#include "../common/protocol.h"
/* Open wire is treated as open/fault; debounce and BLE advertising belong to board integration. */
int door_make_frame(uint8_t is_open, uint16_t seq, uint16_t mv, uint8_t out[MC_FRAME_SIZE]) {
 mc_frame_t f={MC_VERSION,MC_DOOR,seq,MC_TEMP_INVALID,is_open,mv}; return mc_encode(&f,out);
}
