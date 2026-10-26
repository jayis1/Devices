#include <stdint.h>
#include "../common/protocol.h"
/* Zephyr application: bind LIS3DH I2C and GPIO in board overlay; measure RMS in 10 s windows. */
int main(void){static uint16_t seq; ss_record_t t={SS_VERSION,SS_TYPE_TELEMETRY,0,1,0,0,0,0}; uint8_t wire[SS_FRAME_SIZE]; t.sequence=seq++; /* replace these illustrative values with sensor/uptime/ADC reads */ t.uptime_s=0; t.vibration_rms_mg=0; t.battery_mv=3700; return ss_encode(&t,wire);}
