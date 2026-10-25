#include <stdint.h>
void hal_mqtt_publish(const uint8_t *,unsigned);void hal_ble_receive(void);
/* Gateway transport and queue are platform-specific integration points. */
void gateway_step(void){hal_ble_receive();}
