#include <stdint.h>
#include "esp_timer.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

/* ESP-IDF reference loop; sensor driver/calibration intentionally left to board integration. */
void app_main(void) {
    const char *TAG = "courtside-surface";
    i2c_master_bus_config_t bus = {.i2c_port=I2C_NUM_0,.sda_io_num=6,.scl_io_num=7,.clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,.flags.enable_internal_pullup=false};
    i2c_master_bus_handle_t handle;
    esp_err_t rc = i2c_new_master_bus(&bus, &handle);
    if (rc != ESP_OK) { ESP_LOGE(TAG, "I2C init failed: %d", rc); return; }
    for (;;) {
        /* TODO: read ADS1115/SHT40; publish only valid fields, never fabricated zeros. */
        ESP_LOGI(TAG, "sensor sample due at %lld", (long long)(esp_timer_get_time()/1000000));
        vTaskDelay(pdMS_TO_TICKS(300000));
    }
}
