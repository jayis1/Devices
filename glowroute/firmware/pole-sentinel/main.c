/* Board-specific ESP-IDF sensor and BLE integration is intentionally a stub. */
#include "../common/protocol.h"
int main(void){gr_reading sample={1,1,12000};uint8_t frame[GR_FRAME_SIZE];return gr_encode(&sample,frame);}
