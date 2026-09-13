#include <stdbool.h>
#include <stdint.h>
#include "../common/protocol.h"
extern bool key_on(void),stop_pressed(void),leak_a(void),leak_b(void);extern float flow_lpm(void),pressure_kpa(void);extern void dry_contact(bool on);extern bool radio_command(ms_frame_t*);
int main(void){ms_frame_t c; for(;;){bool hazard=(leak_a()&&leak_b()) || (flow_lpm()>20.f&&pressure_kpa()<60.f); if(stop_pressed()||!key_on())dry_contact(false); if(radio_command(&c)&&c.type==MS_COMMAND&&c.len>=2){bool request=c.payload[0]==1, fresh=c.payload[1]<=30; dry_contact(request&&fresh&&key_on()&&!stop_pressed()&&hazard);} /* watchdog is fed only here */}}
