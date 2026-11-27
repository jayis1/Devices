#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "event.h"

/* GPIO bring-up only. Network transport must be provisioned and authenticated
 * before any event may be reported as delivered. */
void app_main(void) {
    gpio_config_t led = {.pin_bit_mask = 1ULL << STEPWISE_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT, .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE, .intr_type = GPIO_INTR_DISABLE};
    gpio_config_t button = {.pin_bit_mask = 1ULL << STEPWISE_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE, .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&led));
    ESP_ERROR_CHECK(gpio_config(&button));
    ESP_LOGI("stepwise", "GPIO ready; protocol %d", STEPWISE_PROTOCOL_VERSION);
    while (true) {
        gpio_set_level(STEPWISE_LED_GPIO, gpio_get_level(STEPWISE_BUTTON_GPIO) == 0);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
