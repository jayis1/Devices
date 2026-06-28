/*
 * PestSync Sensor Types
 * sensor_types.h
 */
#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include "psp_protocol.h"

/* Global shared state */
extern sentinel_data_t  g_sentinel_data;
extern trap_data_t      g_trap_data;
extern deterrent_data_t g_deterrent_data;

/* Battery reading helpers */
uint8_t read_battery_pct_esp32(void);   /* via ADC divider */
uint8_t read_battery_pct_esp32c3(void); /* via ADC divider */

#endif /* SENSOR_TYPES_H */