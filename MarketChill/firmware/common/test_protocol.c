#include <assert.h>
#include <stdint.h>
#include "protocol.h"
int main(void) {
 mc_frame_t a={MC_VERSION,MC_PROBE,0x1234,512,0xff,3000}, b;
 uint8_t bytes[MC_FRAME_SIZE];
 assert(mc_encode(&a,bytes)==0 && mc_decode(bytes,MC_FRAME_SIZE,&b)==0);
 assert(b.sequence==a.sequence && b.temp_centi_c==512 && b.battery_mv==3000);
 assert(mc_decode(bytes,MC_FRAME_SIZE-1,&b)!=0);
 bytes[0]=2; assert(mc_decode(bytes,MC_FRAME_SIZE,&b)!=0);
 a.kind=MC_DOOR; a.temp_centi_c=MC_TEMP_INVALID; a.door_state=1;
 assert(mc_encode(&a,bytes)==0 && mc_decode(bytes,MC_FRAME_SIZE,&b)==0 && b.door_state==1);
 a.door_state=3; assert(mc_encode(&a,bytes)!=0);
 return 0;
}
