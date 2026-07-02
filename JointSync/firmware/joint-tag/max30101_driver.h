/**
 * JointSync Joint Tag — MAX30101 Driver Interface
 *
 * License: MIT
 */

#ifndef MAX30101_DRIVER_H
#define MAX30101_DRIVER_H

#include <stdint.h>
#include "nrf_error.h"

typedef struct {
    uint16_t ir_samples[8];   /* 8 IR samples at 25 Hz */
    uint16_t red_samples[8]; /* 8 Red samples */
    uint8_t  hr;              /* Heart rate (bpm), 0=not computed */
    uint8_t  hrv_ms;          /* HRV RMSSD (ms), 0=not computed */
    uint8_t  spo2;            /* SpO2 (%), 0=not computed */
    uint8_t  confidence;     /* 0-100 */
} max30101_data_t;

nrf_err_t max30101_init(void);
nrf_err_t max30101_read(max30101_data_t *data);
void max30101_shutdown(void);
void max30101_wakeup(void);

#endif /* MAX30101_DRIVER_H */