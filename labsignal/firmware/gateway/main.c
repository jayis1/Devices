#include "../common/protocol.h"
/* RP2040 gateway integration boundary: receive validated frames, cache locally, sync when authorized. */
#define RADIO_SPI_SCK 18
#define RADIO_SPI_MOSI 19
#define RADIO_SPI_MISO 16
#define RADIO_CS 17
int gateway_accept(const uint8_t *frame, size_t len, labsignal_message_t *out) { return labsignal_decode(frame,len,out); }
