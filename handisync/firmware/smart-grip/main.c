#include <stdint.h>
#include <stdbool.h>
typedef struct { int16_t ax,ay,az,gx,gy,gz; uint16_t fsr; } sample_t;
static uint16_t rms_force(const sample_t *s, uint8_t n) { uint32_t q=0; for(uint8_t i=0;i<n;i++) q+=(uint32_t)s[i].fsr*s[i].fsr; return (uint16_t)(q/n); }
void grip_tick(void) { sample_t window[40]; /* read ICM-42688 + SAADC, invoke TFLM feature classifier, notify BLE */ (void)rms_force(window,40); }
