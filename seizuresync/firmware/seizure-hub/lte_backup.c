/* SeizureSync — 4G LTE backup driver (SIM7600G) */
#include "lte_backup.h"
#include "esp_log.h"
static const char *TAG = "LTE";
void lte_backup_init(int tx, int rx, int pwrkey) {
    ESP_LOGI(TAG, "SIM7600G init: tx=%d rx=%d pwrkey=%d", tx, rx, pwrkey);
}
int lte_send_alert(const char *msg) {
    ESP_LOGW(TAG, "LTE backup alert: %s", msg);
    return 0;
}