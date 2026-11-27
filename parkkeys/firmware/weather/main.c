#include <stdint.h>

/* ESP-IDF weather-node pin reference: SHT31 SDA=GPIO6, SCL=GPIO7, rain=GPIO9. */
void app_main(void) {
    volatile uint8_t sht31_address = 0x44;
    (void)sht31_address;
    /* Sensor polling and MQTT transport are not implemented. */
}
