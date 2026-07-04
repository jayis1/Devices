#ifndef GLUCOSYNC_PPG_DRIVER_H
#define GLUCOSYNC_PPG_DRIVER_H

#include <stdint.h>

typedef struct {
    uint8_t hr;           /* heart rate (bpm), 0=invalid */
    uint8_t hrv_rmssd;    /* RMSSD (ms), 0=invalid */
    uint8_t spo2;         /* SpO2 (%), 0=invalid */
    uint8_t confidence;   /* 0-100 */
} ppg_result_t;

typedef enum {
    PPG_RATE_1HZ = 1,
    PPG_RATE_5HZ = 5,
    PPG_RATE_25HZ = 25,
    PPG_RATE_50HZ = 50,
} ppg_sample_rate_t;

void ppg_driver_init(void);
void ppg_driver_set_sample_rate(ppg_sample_rate_t rate);
void ppg_driver_read(ppg_result_t *result);

#endif