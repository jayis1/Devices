/* SeizureSync — BLE 5.0 streaming to hub (ESP-IDF Bluedroid) */
#include "ble_stream.h"
#include "esp_log.h"
#include <string.h>
static const char *TAG = "BLE";

void ble_init(void) {
    ESP_LOGI(TAG, "BLE 5.0 init (production: esp_bluedroid_init + GATT server)");
}

void ble_stream_signal(const float *accel, const float *ppg, const float *eda) {
    /* Production: notify SeizureService SignalChar (0x2A02)
     * Chunk into 200-byte BLE notifications. */
    ESP_LOGD(TAG, "BLE stream: accel+ppg+eda");
    (void)accel; (void)ppg; (void)eda;
}

void ble_stream_chunk(const uint8_t *data, size_t len) {
    ESP_LOGD(TAG, "BLE chunk: %d bytes", (int)len);
    (void)data; (void)len;
}