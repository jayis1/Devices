/* SeizureSync — AuraPatch helpers (nRF52840) */
#ifndef AURANET_PATCH_H
#define AURANET_PATCH_H
#include <stdint.h>
void  twi_init(uint8_t bus, uint8_t scl, uint8_t sda);
void  ble_init_patch(void);
void  ble_send_burst(const uint8_t *data, size_t len);
float tmp117_read_c(void);
float eda_read_uS(void);
float max30101_read_hr(void);
float compute_temp_trend(const float *h, int len);
float compute_eda_trend(const float *h, int len);
float compute_hr_trend(const float *h, int len);
float auranet_predict(const float *temp, const float *eda,
                       const float *hr, int len);
void  eda_init(uint8_t adc_pin);
void  tmp117_init(uint8_t bus);
void  max30101_init(uint8_t bus);
void  send_patch_heartbeat(void);
#endif