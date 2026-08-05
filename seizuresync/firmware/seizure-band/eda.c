/* SeizureSync — AD5940 EDA driver stub */
#include "eda.h"
#include "esp_log.h"
static const char *TAG = "EDA";
void eda_init(int scl, int sda) {
    ESP_LOGI(TAG, "AD5940 init: scl=%d sda=%d", scl, sda);
}
float eda_read_microsiemens(void) { return 0.5f; }