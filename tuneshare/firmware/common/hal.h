#pragma once
#include <stdint.h>
typedef struct { void (*publish)(uint8_t kind, const uint8_t *data, uint16_t len); } ts_hal_t;
