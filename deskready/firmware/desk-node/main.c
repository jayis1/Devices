/* ESP-IDF entry-point placeholder; board GPIO setup must follow the selected module. */
#include "../common/event.h"
#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
void app_main(void) { ESP_LOGI("deskready", "desk node v1: GPIO4 button driver integration required"); for (;;) vTaskDelay(pdMS_TO_TICKS(1000)); }
#else
int main(void) { return 0; }
#endif
