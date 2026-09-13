#include <stdbool.h>
#include "../common/protocol.h"
extern bool radio_receive(ms_frame_t*);extern void mqtt_publish_event(const ms_frame_t*);extern bool policy_accepts(const ms_frame_t*);
int main(void){ms_frame_t f;while(1){if(radio_receive(&f)&&ms_validate(&f,sizeof f,0)){mqtt_publish_event(&f);if(f.type==MS_COMMAND&&!policy_accepts(&f)){} } /* hardware watchdog and local queue service */}}
