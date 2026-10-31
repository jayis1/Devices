/* Platform-neutral observer logic; ESP-IDF sensor/radio binding TODO. */
#include "../common/protocol.h"
#define ON_MA 250u
#define OFF_MA 180u
static uint8_t state=CS_UNKNOWN;
static uint8_t classify(uint16_t ma,int calibrated,int ok){if(!calibrated||!ok||ma==0xffffu)return state=CS_UNKNOWN;if(state!=CS_ACTIVE&&ma>=ON_MA)return state=CS_ACTIVE;if(state==CS_ACTIVE&&ma<=OFF_MA)return state=CS_IDLE;if(state==CS_UNKNOWN)state=CS_IDLE;return state;}
int observer_make_frame(uint32_t id,uint16_t seq,uint16_t ma,int calibrated,int sensor_ok,cs_frame_t*out){if(!out||!id)return-1;out->version=CS_VERSION;out->type=1;out->node_id=id;out->sequence=seq;out->state=classify(ma,calibrated,sensor_ok);out->flags=0;out->sample_ma=ma;return 0;}
