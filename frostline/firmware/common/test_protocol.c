#include "protocol.h"
#include <assert.h>
int main(void){
 fl_telemetry_t a={1,1,42,7,1730000000,-125,315,810,1,3650,0},b={0}; uint8_t f[FL_FRAME_LEN];
 assert(fl_encode(&a,f)==0); assert(fl_decode(f,sizeof f,&b)==0);
 assert(b.node_id==a.node_id&&b.sequence==a.sequence&&b.surface_temp_c_centi==-125&&b.unix_time_s==a.unix_time_s);
 f[10]^=1u; assert(fl_decode(f,sizeof f,&b)<0); assert(fl_decode(f,sizeof f-1,&b)<0); return 0;
}
