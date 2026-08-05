/* SeizureSync — AuraPatch helper stubs (nRF52840) */
#include "auranet_patch.h"
#include <string.h>

void twi_init(uint8_t bus, uint8_t scl, uint8_t sda) {
    (void)bus; (void)scl; (void)sda;
}
void ble_init_patch(void) {}
void ble_send_burst(const uint8_t *data, size_t len) {
    (void)data; (void)len;
}
float tmp117_read_c(void) { return 36.5f; }
float eda_read_uS(void) { return 0.5f; }
float max30101_read_hr(void) { return 72.0f; }
void eda_init(uint8_t adc_pin) { (void)adc_pin; }
void tmp117_init(uint8_t bus) { (void)bus; }
void max30101_init(uint8_t bus) { (void)bus; }