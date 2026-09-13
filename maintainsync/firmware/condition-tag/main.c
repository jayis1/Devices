#include <stdint.h>
#include "../common/protocol.h"
extern void bmi270_read(int16_t *xyz); extern int16_t tmp117_centi_c(void); extern uint16_t adc_read(void); extern void radio_send(ms_frame_t*);
int main(void){uint32_t seq=0; for(;;){int16_t a[3]; bmi270_read(a); ms_frame_t f={.version=1,.type=MS_TELEMETRY,.node_id=0x1001,.seq=++seq,.len=10}; ((int16_t*)f.payload)[0]=a[0];((int16_t*)f.payload)[1]=a[1];((int16_t*)f.payload)[2]=a[2];((int16_t*)f.payload)[3]=tmp117_centi_c();((uint16_t*)f.payload)[4]=adc_read(); f.crc32c=ms_crc32c((uint8_t*)&f,offsetof(ms_frame_t,crc32c));radio_send(&f);/* sleep 60 s; IMU ISR wakes for event */}}
