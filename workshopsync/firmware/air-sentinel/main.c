/* Air Sentinel portable state machine — authored by jayis1. */
#include <stdint.h>
typedef struct { uint16_t pm25_ugm3; uint16_t voc_index; uint16_t airflow_lpm; uint8_t quality; } air_sample_t;
uint8_t air_requires_advisory(const air_sample_t *s) { return s && (s->pm25_ugm3>35u || s->voc_index>150u || s->airflow_lpm<10u); }
int main(void) { air_sample_t sample={0,0,0,0}; (void)air_requires_advisory(&sample); for(;;) { /* ESP-IDF driver sampling and TDMA telemetry */ } }
