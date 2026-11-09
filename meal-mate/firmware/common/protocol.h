#ifndef MEALMATE_PROTOCOL_H
#define MEALMATE_PROTOCOL_H
#include <stdint.h>
#define MM_VERSION 1u
#define MM_MAX_PAYLOAD 96u
typedef enum { MM_WEIGHT=1, MM_CLIMATE=2, MM_DOOR=3 } mm_kind_t;
typedef struct {
  uint8_t version, kind;
  uint16_t payload_len;
  uint32_t device_id, sequence, uptime_s;
  int32_t value_a, value_b; /* milli-grams, milli-Celsius / milli-RH-percent */
  uint16_t crc16;
} mm_event_t;
/* Transport must authenticate peer and verify CRC before parsing; sequence is monotonic per device. */
#endif
