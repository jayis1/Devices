#include "../common/protocol.h"
/* ESP-IDF adapter placeholder: scan BLE frames, validate, spool and expose local API. */
int hub_accept_frame(const uint8_t *frame, size_t length) { return nn_validate(frame,length); }
