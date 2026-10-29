#include <stdint.h>
#include "../common/protocol.h"

#define SUPPLY_HX711_DOUT_GPIO 4
#define SUPPLY_HX711_SCK_GPIO 5

int app_main(void) {
    /* ESP-IDF integration skeleton: tare/calibrate HX711, sample stable mass,
       then publish protocol-v1 supply JSON via site MQTT. */
    return -1;
}
