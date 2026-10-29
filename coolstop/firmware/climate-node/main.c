#include <stdint.h>
#include "../common/protocol.h"

/* ESP-IDF integration skeleton: bind SHT31 I2C driver, Wi-Fi and MQTT in board project. */
#define CLIMATE_SDA_GPIO 4
#define CLIMATE_SCL_GPIO 5
#define CLIMATE_SHT31_ADDRESS 0x44

int app_main(void) {
    /* Initialize I2C, sample SHT31, then publish validated v1 climate JSON.
       This placeholder intentionally fails until the ESP-IDF board adapter is supplied. */
    return -1;
}
