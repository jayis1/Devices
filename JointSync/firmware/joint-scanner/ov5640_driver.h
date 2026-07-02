/**
 * JointSync Joint Scanner — OV5640 Driver Interface
 *
 * License: MIT
 */

#ifndef OV5640_DRIVER_H
#define OV5640_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

esp_err_t ov5640_init(void);
esp_err_t ov5640_capture_qvga(uint8_t *buf, size_t buf_len);
esp_err_t ov5640_capture_raw(uint8_t *buf, size_t buf_len, size_t *actual_len);
void ov5640_set_exposure(int level);

#endif /* OV5640_DRIVER_H */