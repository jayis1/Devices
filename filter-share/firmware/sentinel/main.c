#include "../common/protocol.h"
/* ESP-IDF adapter required: read SPS30 UART and transmit encoded frame over ESP-NOW. */
#define SENSOR_RX_GPIO 20
#define SENSOR_TX_GPIO 21
#define STATUS_LED_GPIO 8
int sentinel_make_frame(uint8_t *out, uint32_t seq, uint32_t uptime_ms, int32_t pm25_x10, int32_t pm10_x10) {
  return fs_encode(out, 1, 0x534e5401u, seq, uptime_ms, pm25_x10, pm10_x10);
}
