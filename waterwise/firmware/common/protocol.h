#ifndef WATERWISE_PROTOCOL_H
#define WATERWISE_PROTOCOL_H
#include <stdint.h>
#define WW_PROTOCOL_VERSION 1
#define WW_MAX_PAYLOAD 512
/* Wire-independent telemetry record; serialize with a bounded JSON encoder. */
typedef struct {
  uint8_t version;
  char node_id[24];
  uint32_t sequence;
  uint64_t timestamp_s;
  float value;
  char kind[12];
  char unit[8];
} ww_telemetry_t;
/* Returns 1 only for valid finite-domain flow/temperature samples. */
int ww_valid(const ww_telemetry_t *m);
#endif
