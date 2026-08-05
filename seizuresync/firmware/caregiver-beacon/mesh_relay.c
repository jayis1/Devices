/* SeizureSync — Caregiver Beacon mesh relay (stub) */
#include "mesh_relay.h"
#include "esp_log.h"
static const char *TAG = "RELAY";
void mesh_relay_init(void) { ESP_LOGI(TAG, "Mesh relay init"); }
int  mesh_relay_packet(const uint8_t *data, size_t len) {
    ESP_LOGD(TAG, "Relay %d bytes", (int)len);
    (void)data; (void)len;
    return 0;
}