/* SeizureSync — BLE 5.0 streaming to hub (header) */
#ifndef BLE_STREAM_H
#define BLE_STREAM_H
void ble_init(void);
void ble_stream_signal(const float *accel, const float *ppg, const float *eda);
void ble_stream_chunk(const uint8_t *data, size_t len);
#endif