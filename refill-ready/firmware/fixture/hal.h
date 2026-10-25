#include <stdint.h>
/* HAL seam: target integration supplies these functions. */
uint32_t hal_uptime_seconds(void); uint16_t hal_flow_pulses_10s(void); int hal_leak_active(void); void hal_ble_send(const uint8_t *, unsigned);
