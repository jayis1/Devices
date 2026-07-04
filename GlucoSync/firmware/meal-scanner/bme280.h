#ifndef GLUCOSYNC_BME280_H
#define GLUCOSYNC_BME280_H

#include <stdint.h>

void bme280_init(void);
void bme280_read(float *temp_c, float *humidity_pct);

#endif