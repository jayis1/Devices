#include <stdint.h>
#include "esp_log.h"
#include "esp_event.h"

/* Gateway application skeleton: BLE central provisioning and HTTPS queue are not yet implemented. */
void app_main(void) {
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI("courtside-gateway", "boot; provision site and trusted BLE peers before deployment");
    for (;;) vTaskDelay(pdMS_TO_TICKS(10000));
}
