#include "../common/protocol.h"
/* RTD conversion and wetness ADC acquisition are target-HAL responsibilities. */
int surface_probe_make_report(uint16_t id,uint16_t seq,uint32_t epoch,int16_t surface_centi,uint16_t wet,uint16_t battery,uint8_t out[FL_FRAME_LEN]) {
 fl_telemetry_t v={FL_VERSION,1u,id,seq,epoch,surface_centi,INT16_MIN,0u,wet,battery,0u};
 return fl_encode(&v,out);
}
