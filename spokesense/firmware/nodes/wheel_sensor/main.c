#include "../common/protocol.h"
/* ESP-IDF integration point: initialize BMI270 I2C (SDA6/SCL7), sample during a manually rotated wheel, then publish over commissioned BLE. */
int main(void){ss_message sample={.node="wheel-1",.kind=1,.value_milli=0};(void)sample;/* sensor/radio SDK implementation is not included or hardware-tested */return 0;}
