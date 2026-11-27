/* ESP-IDF entry-point placeholder; SHT31 I2C driver integration required. */
#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
void app_main(void) { ESP_LOGI("deskready", "room node v1: SHT31 driver integration required"); for (;;) vTaskDelay(pdMS_TO_TICKS(5000)); }
#else
int main(void) { return 0; }
#endif
