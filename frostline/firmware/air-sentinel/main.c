#include "../common/protocol.h"
/* Sensor acquisition and radio calls are supplied by the target HAL. */
int air_sentinel_make_report(uint16_t id,uint16_t seq,uint32_t epoch,int16_t air_centi,uint16_t rh,uint16_t battery,uint8_t out[FL_FRAME_LEN]) {
 fl_telemetry_t v={FL_VERSION,1u,id,seq,epoch,INT16_MIN,air_centi,rh,0u,battery,0u};
 return fl_encode(&v,out);
}
