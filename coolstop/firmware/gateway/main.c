#include <stdint.h>
#include "../common/protocol.h"

#define GATEWAY_OLED_SDA_GPIO 8
#define GATEWAY_OLED_SCL_GPIO 9
#define GATEWAY_OLED_I2C_ADDRESS 0x3C

int app_main(void) {
    /* ESP-IDF integration skeleton: receive local node freshness via MQTT and
       render latest readings on OLED; never display missing values as ready. */
    return -1;
}
