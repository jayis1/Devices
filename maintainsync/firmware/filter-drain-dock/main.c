#include <stdint.h>
#include "../common/protocol.h"
extern int16_t sdp810_pa(void);extern uint8_t float_state(void);extern void radio_send(ms_frame_t*);
int main(void){uint32_t seq=0;for(;;){ms_frame_t f={.version=1,.type=MS_TELEMETRY,.node_id=0x4001,.seq=++seq,.len=3};*(int16_t*)f.payload=sdp810_pa();f.payload[2]=float_state();f.crc32c=ms_crc32c((uint8_t*)&f,offsetof(ms_frame_t,crc32c));radio_send(&f);/* RTOS delay 60 s */}}
