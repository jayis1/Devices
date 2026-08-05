/* SeizureSync — ICM-42688-P driver stub */
#include "accel.h"
#include "esp_log.h"
#include <math.h>
static const char *TAG = "ACCEL";
void accel_init(int cs, int sck, int miso, int mosi, int int1) {
    ESP_LOGI(TAG, "ICM-42688-P init: cs=%d sck=%d miso=%d mosi=%d int1=%d",
             cs, sck, miso, mosi, int1);
}
float accel_read_magnitude(void) {
    /* Production: SPI read 0x0B-0x10 (ACCEL_DATA_XYZ), compute magnitude */
    return 1.0f;   /* 1 g resting */
}