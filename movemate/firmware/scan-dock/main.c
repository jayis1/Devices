#include "../common/protocol.h"
/* ESP-IDF integration TODO: QR UART, display and BLE pairing. */
int movemate_scan_dock_protocol_ready(void){mm_message m={.version=1,.type=2,.length=0,.sequence=1};uint8_t frame[140];size_t n=0;return mm_encode(&m,frame,sizeof frame,&n);}
