/* SeizureSync — MAX30102 PPG driver stub */
#include "ppg.h"
#include "esp_log.h"
static const char *TAG = "PPG";
void ppg_init(int scl, int sda) {
    ESP_LOGI(TAG, "MAX30102 init: scl=%d sda=%d", scl, sda);
}
float ppg_read_hr(void) { return 72.0f; }
int   ppg_read_spo2(void) { return 97; }